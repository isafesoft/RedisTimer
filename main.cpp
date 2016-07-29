#include<iostream>
#include<pthread.h>
#include<unistd.h>
#include <signal.h>
#include <cstddef>
#include <cstdlib>
#include <errno.h>
#include <sys/wait.h>
#include <regex.h>
#include <cstring>
#include <sys/syslog.h>
#include "./hiredis/hiredis.h"
#include "conf.h"

static int g_cmdCount = 0;
static int g_timeHour = 0;
static int g_timeDay = 0;
static int g_logcount = 0;

#define ERR_OK  0
#define ERR_RDS_CONNECT 0X10

#define SZ_CMDCOUNT_TAG ("total_commands_processed:")
#define SZ_TRACKINGPOINT ("TRACINGPOINT_CATEGORY")

#define SZ_PATH_PHPBIN ("/home/pubsrv/php-5.5.10/bin/php")
#define SZ_PATH_PHPFILE ("/home/xujiantao/ewr/scripts/Persistence.php")
#define ERRRETURN(a) (a == NULL ? true : false) 
#define PRINTCMDERR printf("Err: command err!");
#define SAFE_FREE_REPLY(a)  if(a != NULL){ freeReplyObject(a); a = NULL;}

//#define SZ_PATH_PHPBIN ("/usr/local/bin/php")
//#define SZ_PATH_PHPFILE ("/usr/local/bin/test.php")

#define CLR_HOURLY 0x10
#define CLR_DAILY 0x11

struct conf *g_cfg;

using namespace std;

bool findCmdCount(string &cmd)
{
    bool res = false;
    int ib = cmd.find(SZ_CMDCOUNT_TAG);
    if (-1 == ib)
    {
        return res;
    }

    int ie = cmd.find("\r\n", ib);
    if(-1 == ie)
    {
        return res;
    }
    ib += strlen(SZ_CMDCOUNT_TAG);
    string strcount = cmd.substr(ib, ie - ib);
    cmd = strcount;
    res = true;
    return res;

}

bool checkUpdated(redisContext *conn, int &count)
{
    if(NULL == conn)
    {
        return false;
    }
    bool res = false;
    redisReply *reply = NULL;
    reply = (redisReply *)redisCommand(conn, "info stats");
	if(ERRRETURN(reply))
	{
		PRINTCMDERR;
		return false;
	}

    //printf("key type, %d\n", reply->type);

    do{
        if(REDIS_REPLY_STRING != reply->type)
        {
            printf("get info error! %d\n", reply->type);
            res = false;
            break;
        }

        string cmd = reply->str;
        res = findCmdCount(cmd);
        if (false == res)
        {
            break;
        }

        count = atoi(cmd.c_str());
        //printf("counts %d\n", count);
        res = true;
    }while(false);

	SAFE_FREE_REPLY(reply);
    return res;
}

int testCallFork()
{
    pid_t pid;
    int code = ERR_OK;
    if ( (pid = fork()) == 0 )
    {
        //execlp("/home/root/cpp/testfork/testfork", NULL);
        execl(SZ_PATH_PHPBIN, "php", SZ_PATH_PHPFILE, "counter","tracingpoint",  NULL);
        //execl("/usr/bin/awk", "awk",NULL);
        /* 如果exec函数返回，表明没有正常执行命令，打印错误信息*/
        char errmsg[1024] = "";
        sprintf(errmsg, "1, fork but new sub error=%d, pid=%d\n", errno, getpid());
        perror(errmsg);
        code = errno;
        kill(getpid(), SIGINT);
    }
    else if(pid < 0)
    {
        printf("2,fork error!, pid=%d\n", getpid());
        code = 1;
        //kill(getpid(), SIGTERM);
    }
    else
    {
        int res = 0;
        printf("wait..., pid=%d", getpid());
        wait(&res);
        code = res >> 8;
        printf("\n3,fork with code:%d, pid=%d\n", res >> 8, getpid());
    }
    return code;
}

string _get_show_time(timeval tv) {
    char show_time[40];
    memset(show_time, 0, 40);

    struct tm *tm;
    tm = localtime(&tv.tv_sec);

    sprintf(show_time, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec/1000));
    return std::string(show_time);
}

string _get_show_time() {
    struct timeval now;
    struct timezone tz;
    gettimeofday(&now, &tz);
    std::string fin_format = _get_show_time(now);
    return fin_format;
}

int _getHour()
{
    struct timeval now;
    struct timezone tz;
    gettimeofday(&now, &tz);
    struct tm *tm;
    tm = localtime(&now.tv_sec);
    return tm->tm_hour;
}

int _getDay()
{
    struct timeval now;
    struct timezone tz;
    gettimeofday(&now, &tz);
    struct tm *tm;
    tm = localtime(&now.tv_sec);
    return tm->tm_mday;
}

int clearRedis(redisContext *conn, int clrType = CLR_HOURLY)
{
    if(NULL == conn)
        return ERR_RDS_CONNECT;

    redisReply *reply = NULL;

    do
    {
        if(CLR_DAILY == clrType)
        {
            reply = (redisReply *)redisCommand(conn,"flushall");
			if(ERRRETURN(reply))
			{
				PRINTCMDERR;
				break;
			}
            printf("********EWR:Ok, Delete daily redis OK\n");
			SAFE_FREE_REPLY(reply);
            break;
        }

        reply = (redisReply *)redisCommand(conn,"keys *");
		if(ERRRETURN(reply))
		{
			PRINTCMDERR;
			break;
		}
		
        if(REDIS_REPLY_ARRAY == reply->type)
        {
            int count = reply->elements;
            for (int i = 0; i < count; ++i) {

                redisReply *tmpreply = reply->element[i]; //
                string key = tmpreply->str;
                //freeReplyObject(tmpreply); //delete this replyobject element!

                //check trackingpoint type
                if(-1 != key.find(SZ_TRACKINGPOINT))
                {
                    continue;
                }

                //
                string cmd = "del ";
                cmd += key;
                redisReply *delReply = (redisReply *)redisCommand(conn, cmd.c_str());
                if(delReply->type == REDIS_REPLY_INTEGER && delReply->integer == 1)
                {
                    syslog(LOG_ALERT, "EWR:Ok, %s\n", cmd.c_str());
                }
                //freeReplyObject(delReply);

            }//for

			SAFE_FREE_REPLY(reply);
        }//if
    }while(false);



    return ERR_OK;
}

void *th_fn(void *arg)
{
    redisContext *conn  = redisConnect("127.0.0.1",6379);
    if (conn == NULL || conn->err) {
        if (conn) {
            printf("Error: %s\n", conn->errstr);
            pthread_exit((void *)0);
            // handle error
        } else {
            printf("Can't allocate redis context\n");
            pthread_exit((void *)0);
        }
    }
    g_timeHour = _getHour(); // init hour
    g_timeDay = _getDay(); //init day
	
    while(1)
    {
        //redisReply *reply = (redisReply*)redisCommand(conn,"set foo 1234");
        //freeReplyObject(reply);

        //reply = (redisReply *)redisCommand(conn,"get foo");
        //freeReplyObject(reply);

        int cmdcount = 0;
        int resStore = 0;
        string strtm = _get_show_time();
        //get the lastest processed comand count,
        bool res = checkUpdated(conn, cmdcount);
        printf("\n\n>%d %s pid:%d cmdcount:%d\n---------------------------\n",++g_logcount, strtm.c_str(), getpid(), cmdcount);
        if(!res)
        {
            printf("checkupdate false!\n");
        }

        if(cmdcount - 1 > g_cmdCount)
        {
            //need store new incoming data
            printf("\tEWR: NEED STRORE!, newcount: %d, oldcount:%d\n", cmdcount, g_cmdCount);

            resStore = testCallFork();
            if(resStore != ERR_OK)
            {
                printf("\tEWR: ERROR, store redis to mysql! %d\n", resStore);
            }
            else
            {
				printf("\tEWR: OK, store redis to mysql! %d\n", resStore);

                //clear redis hourly count
                printf("\tEWR: [g_timeHour]=%d, [nowHour]=%d\n", g_timeHour, _getHour());
                if(g_timeHour != _getHour())
                {
                    //clear redis!
                    printf("EWR: Action------>clear redis hourly!\n");
                    g_timeHour = _getHour();
                    clearRedis(conn, CLR_HOURLY);
                }
				else
				{
					printf("EWR: Action------>NO NEED clear hourly now!\n");
				}

                printf("\nEWR: [g_timeDay]=%d, [nowDay]=%d\n", g_timeDay, _getDay());
                //clear redis daily count
                if(g_timeDay != _getDay())
                {
                    printf("EWR: Action---->>clear redis daily!\n");
                    clearRedis(conn, CLR_DAILY);
                    g_timeDay = _getDay();
                }
				else
				{

					printf("EWR: Action---->>NO NEED clear daily  now!\n");
				}
            }//else
        }//if
        else
        {
            printf("$$EWR: NO NEED call php, new cmdcount:%d, oldcount:%d\n\n", cmdcount, g_cmdCount);
        }

        //update cmd count
        checkUpdated(conn, g_cmdCount);
        //cout<<"new thread"<<endl;
        //testCallFork();
        //break;testCallFork
        sleep(6);
        //pthread_t tid = pthread_self();
        //pthread_exit((void *)0);

    }
    SAFE_FREE_REPLY(conn);
    return 0;

}

void sigreload(int sig) {
    printf("receive sig:%d \n", sig);
}

static void
server_handle_signal(int id) {
    int threadid = pthread_self();
    switch(id) {
        case SIGHUP:
            //slog_init(__server);
            printf("receive SIGHUP...\n");
            break;
        case SIGTERM:
        case SIGINT:
            //slog(__server, WEBDIS_INFO, "Webdis terminating", 0);
            printf("receive SIGINT...\n");
            //pthread_exit((void *)2);
            //exit(0);
            break;
        case SIGUSR1:

            sigreload(id);
            break;
        default:
            break;
    }
}

static void
server_install_signal_handlers() {

    signal(SIGHUP,  server_handle_signal);
    signal(SIGTERM, server_handle_signal);
    //signal(SIGINT,  server_handle_signal);
    signal(SIGUSR1,  server_handle_signal);
}

int main(int argc, char *argv[]) {
    if (argc > 1)
    {
        g_cfg = conf_read(argv[1]);
    }
    else
    {
        g_cfg = conf_read(CFG_DEFAULT_CONF);
    }

    pthread_t ptid;
    void *tret;
    server_install_signal_handlers();

    pthread_create(&ptid, NULL,th_fn ,NULL);

    cout << "wait pthread_join" << endl;
    pthread_join(ptid, &tret);
    cout << "join returned" << endl;

    conf_free(g_cfg);
    return 0;
    //cout<<"code 2 exit id = "<<(int)tret<<endl;
}
