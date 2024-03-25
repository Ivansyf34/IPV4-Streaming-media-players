CFLAGS+=-I../include/  -g
LDFLAGS+=-pthread

#找到当前目录下所有的.c文件
obj:= $(wildcard *.c)
#且不是文件夹
obj:=$(notdir $(obj))
#将.c文件替换成.o文件
obj:= $(patsubst %.c,%.o,$(obj))

#过滤不需要的依赖文件
filterobj:=
#生成的可执行文件名称
Exe:= server

#如果定义了filterobj变量，就过滤掉filterobj变量中的依赖文件
ifndef $(filterobj)
	obj:= $(filter-out $(flilterobj),$(obj))
endif

#如果父makefile没有定义baseUrl变量，就定义baseUrl变量
ifndef baseUrl:
	baseUrl:= ../../
endif
binUrl:= $(baseUrl)bin/

#伪目标
.PHONY: all cleanObj cleanExe clean

#生成可执行文件
all:$(Exe)
$(Exe): $(obj)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	cp $(Exe) $(binUrl)
#静态模式
$(obj): %.o: %.c
	$(CC) -c $< $(CFLAGS)


clean: cleanObj cleanExe

cleanObj:
	rm -rf *.o

cleanExe:
	rm $(Exe)


