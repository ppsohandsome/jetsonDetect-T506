## 程序适用

T506盒子,如需在T600上使用，替换lib目录下的libjetson-utils.so
具体为：将  change t600  目录下的文件覆盖到lib目录



## 安装说明

### 1.手动下载文件

```
libopencv_cudafilters.so
test2.mp4 

这两个文件超出github大小，需要手动下载并放在目录中
下载地址在下面的超链接
```

[下载地址](https://pan.baidu.com/s/1ytwyb7WFQwscgDT4pPcSMQ?pwd=3ztm)



### 2.解压+将下载的文件复制到对应目录

cd到盒子里的文件存放位置（以downloads为例）

```
cd
cd Downloads
```

放到~/project文件夹

```
mv jetsonDetect.tar.gz ~/project/
```

进入project并解压jetsonDetect.tar.gz

```
cd
cd project
tar -zxvf jetsonDetect.tar.gz
```

下载的两个文件放到对应位置

```
libopencv_cudafilters.so  ----->lib/
test2.mp4                 ----->videos/
```

### 3.配置文件修改

配置文件路径

```
vi config/config.json
```

#### 3.1修改摄像头地址

以单个摄像头为例：
需要修改的为 摄像头id，rtsp地址，高和宽

```
{
                "camID": "41",
                "rtspPath": "rtsp://admin:abcd1234@192.168.1.107:554/h264/ch1/main/av_stream",
                "width": 1920,
                "height": 1080
},
```

#### 3.2修改kafka配置

enable=true/false：是否传给kafka

ip：kafka服务器地址

port：端口

topic：topic名称

user和password不用管

```
"KafkaConfig": {
      "enable": true,
      "ip": "192.168.1.13",
      "port": 9092,
	  "topic":"idtcz_3",
	  "user": "admin",
      "password": "123123"
    },
```

### 4.修改c++外部库地址

打开conf，添加红框这一行

```
 sudo vi /etc/ld.so.conf
```

![image-20241101155716306](assets/image-20241101155716306.png)

刷新conf文件

```
sudo ldconfig
```

### 5.运行测试

三个sh文件可控制重启

```
./jetson_check.sh
./jetson_restart.sh
./jetson_stop.sh
```

进入新程序目录

```
cd /home/nvidia/project/jetsonDetect
```

连续输入两次，看到pid即为运行成功

```
./jetson_check.sh
```

![image-20241101160041170](assets/image-20241101160041170-17345895673944.png)

### 6.log检查

进入log目录

```
cd /home/nvidia/project/jetsonDetect/log
```

ll查看文件列表

![image-20241101160200663](assets/image-20241101160200663.png)

tail -f实时查看文件（今日日期），以11.1为例

```
tail -f jetsonDetect_2024-11-01.log
```

等几分钟，如果出现object，则为测试结束

![image-20241101160333109](assets/image-20241101160333109.png)

### 7.增加守护进程

任意目录进入定时器

```
crontab -e
```

输入下面的内容

```
#每分钟检查pid是否存在
* * * * * /home/nvidia/project/jetsonDetect/jetson_check.sh
#每日的00:00 杀死pid并重启
0 0 * * * /home/nvidia/project/jetsonDetect/jetson_restart.sh
```

手动stop程序，打开log等待几分钟，如果出现object（程序被自动check），则添加成功

```
cd /home/nvidia/project/jetsonDetect/
###stop程序####
./jetson_stop.sh
```

## 至此更新完毕

## 记一次报错

程序运行大量输出gstreamer的解码信息

最终问题所在：t600编译的jetson-util不适用于t506

解决方案：在t506搭建环境重新编译jetson-util
