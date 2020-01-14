# 模拟信道
 这是用Python2开发的模拟信道，采用了异步协程库Tornado开发

## 开启模拟信道
 1. 确保自己系统已经安装了Python2 以及 软件包管理工具pip ， virtualenv
 2. 通过git克隆工程下来，进入目录，执行`virtualenv --no-site-packages venv`, 然后进入虚拟环境：`source venv/bin/activate`, 接着，安装必要的库：`pip install -r requirement.txt`
 3. 运行程序 `python tornadoChannel.py`
 4. 通过指令 `python tornadoChannel.py -h`，查看可以输入的变量
