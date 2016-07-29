#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "./jassonobj/src/jansson.h"
#include "conf.h"


struct conf *
conf_read(const char *filename) {

	json_t *j;
	json_error_t error;
	struct conf *conf;
	void *kv;

	/* defaults */
	conf = (struct conf *)calloc(1, sizeof(struct conf));
	conf->redis_host = strdup("127.0.0.1");
	conf->redis_port = 6379;
	conf->http_host = strdup("0.0.0.0");
	conf->http_port = 7379;
	conf->http_max_request_size = 128*1024*1024;
	conf->http_threads = 4;
	conf->user = getuid();
	conf->group = getgid();
	conf->logfile = CFG_DEFAULT_CONF;
	conf->daemonize = 0;
	conf->pidfile = "webdis.pid";
	conf->database = 0;
	conf->pool_size_per_thread = 2;

	j = json_load_file(filename, 0, &error);
	if(!j) {
		fprintf(stderr, "Error: %s (line %d)\n", error.text, error.line);
		return conf;
	}

	for(kv = json_object_iter(j); kv; kv = json_object_iter_next(j, kv)) {
		json_t *jtmp = json_object_iter_value(kv);

		if(strcmp(json_object_iter_key(kv), "redis_host") == 0 && json_typeof(jtmp) == JSON_STRING) {
			free(conf->redis_host);
			conf->redis_host = strdup(json_string_value(jtmp));
		} else if(strcmp(json_object_iter_key(kv), "redis_port") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->redis_port = (short)json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "redis_auth") == 0 && json_typeof(jtmp) == JSON_STRING) {
			conf->redis_auth = strdup(json_string_value(jtmp));
		} else if(strcmp(json_object_iter_key(kv), "http_host") == 0 && json_typeof(jtmp) == JSON_STRING) {
			free(conf->http_host);
			conf->http_host = strdup(json_string_value(jtmp));
		} else if(strcmp(json_object_iter_key(kv), "http_port") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->http_port = (short)json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "http_max_request_size") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->http_max_request_size = (size_t)json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "threads") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->http_threads = (short)json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "user") == 0 && json_typeof(jtmp) == JSON_STRING) {
			struct passwd *u;
			if((u = getpwnam(json_string_value(jtmp)))) {
				conf->user = u->pw_uid;
			}
		} else if(strcmp(json_object_iter_key(kv), "group") == 0 && json_typeof(jtmp) == JSON_STRING) {
			struct group *g;
			if((g = getgrnam(json_string_value(jtmp)))) {
				conf->group = g->gr_gid;
			}
		} else if(strcmp(json_object_iter_key(kv),"logfile") == 0 && json_typeof(jtmp) == JSON_STRING){
			conf->logfile = strdup(json_string_value(jtmp));
		} else if(strcmp(json_object_iter_key(kv), "daemonize") == 0 && json_typeof(jtmp) == JSON_TRUE) {
			conf->daemonize = 1;
		} else if(strcmp(json_object_iter_key(kv),"pidfile") == 0 && json_typeof(jtmp) == JSON_STRING){
			conf->pidfile = strdup(json_string_value(jtmp));
		} else if(strcmp(json_object_iter_key(kv), "websockets") == 0 && json_typeof(jtmp) == JSON_TRUE) {
			conf->websockets = 1;
		} else if(strcmp(json_object_iter_key(kv), "database") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->database = json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "pool_size") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->pool_size_per_thread = json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "default_root") == 0 && json_typeof(jtmp) == JSON_STRING) {
			conf->default_root = strdup(json_string_value(jtmp));
		}
	}

	json_decref(j);

	return conf;
}


void
conf_free(struct conf *conf) {

	free(conf->redis_host);
	free(conf->redis_auth);

	free(conf->http_host);

	free(conf);
}
