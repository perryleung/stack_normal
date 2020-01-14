# Stack V1.3.0 快速运行指南

### 1 安装需要的库
- sudo apt-get install libyaml-cpp-dev
- sudo apt-get install libboost-all-dev

### 2 安装evlib库
1. 下载最[新版本](http://dist.schmorp.de/libev/)【链接下载的是4.24版本】
2. 解压
3. 进入目录运行后：
```
sh autogen.sh
./configure && make
sudo make install
```
执行第一句若出现
```
/autogen.sh: 4: autoreconf: not found
```
就安装
```
sudo apt-get install autoconf automake libtool
```
4. 检查是否成功安装
```
ls -al /usr/local/lib |grep libev  (若你没有指定安装目录，则默认是/usr/local/lib 路径)
```

### 3 修改配置文件
复制congig.example.yaml并作相应的修改,修改包括clientsocket的address，drt的nodeID，server的port等。
需要开启随机发送数据包测试模式的话，需要将testmode置为1,并填写seed：随机数种子，timeinterval：数据包发送间隔，packetnum：数据包发送次数，othernodeID：这个不需要设置。
需要开启trace记录的话，需要将tracemode置为1，填写好trace文件的目录以及trace文件的名字，每一个进程的trace文件名必须不同。

### 4 make

### 5 关于项目的分支结构，查看项目的wiki

### 6 开发前记得在master中git pull一下，再在dev分支中git merge master一下

### 7 关于协议配置文件moduleconfig.h文件的使用

增加新协议的时候，可以参照[mac/aloha.cc文件](https://gitlab.com/scut-underwater/stack/blob/master/mac/aloha.cc)，只需保留一个**current_pid**的宏定义，其他宏定义都省去，同时在文件最下面加上条件编译。
使用的时候，在moduleconfig文件中加上需要使用的协议的**id**好，然后对工程进行**make clean**后，再**make**一下即可。

