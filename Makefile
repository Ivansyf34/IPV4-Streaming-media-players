export baseUrl:= $(shell pwd)/

.PHONY: all clean cleanBin cleanOther

all:
	@echo 删除bin目录
	-rm -rf bin
	@echo 创建bin目录
	-mkdir bin
	@echo "开始编译client"
	@cd src/client && make
	@echo "开始编译server"
	@cd src/server && make
	@echo "编译完成"

clean: cleanBin cleanOther

cleanBin:
	@echo 删除bin目录
	-rm -rf bin

cleanOther:
	@echo "开始清理client"
	@cd src/client && make clean
	@echo "开始清理server"
	@cd src/server && make clean
	@echo "清理完成"