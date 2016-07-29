objects = dump.o error.o hashtable.o load.o strbuffer.o utf.o value.o variadic.o

#jasson : $(objects)
#	cc -o jasson $(objects)

libobj = libjassonjw.a
target : $(objects)
	cc  $(objects) -shared -o  $(libobj)

dump.o : jansson.h jansson_private.h strbuffer.h utf.h   
error.o : jansson_private.h	
hashtable.o : 	jansson_config.h jansson_private.h hashtable.h
load.o : jansson.h jansson_private.h strbuffer.h utf.h
strbuffer.o : jansson_private.h strbuffer.h
utf.o : utf.h 
variadic.o : jansson.h jansson_private.h 
value.o : jansson.h hashtable.h jansson_private.h utf.h

clean : 
	rm jasson $(objects)
