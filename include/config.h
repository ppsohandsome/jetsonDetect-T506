//kafka相关配置
typedef struct {
	bool enable;
	std::string ip;
	int port;
	std::string topic;
	std::string user;
	std::string password;
}KAFKACONFIG;

//相机相关配置
typedef struct {
	std::string camID;
	std::string rtspPath;
	int width;
	int height;
}RTSPCONFIG;

//label相关配置
typedef struct {
	int class_num;
	std::vector<std::string> labelList;
}LABELCONFIG;

//总配置
typedef struct {
	KAFKACONFIG kafkaconfig;
	std::vector<RTSPCONFIG> rtspList;
	LABELCONFIG labelconfig;
}CONFIG;

//获取时间戳
std::string getCurrentTimeStr(){
	time_t t = time(NULL);
	char ch[64] = {0};
	char result[100] = {0};
	strftime(ch, sizeof(ch) - 1, "%Y%m%d--%H%M%S", localtime(&t));
	sprintf(result, "%s", ch);
	return std::string(result);
}

//日志输出
void logout(std::string str1){
    std::cout << getCurrentTimeStr() << std::endl;
    std::cout << str1 << std::endl;
    std::cout << "===============================================" << std::endl;
}
void logout(std::string str1,std::string str2){
    std::cout << getCurrentTimeStr() << std::endl;
    std::cout << str1 << std::endl;
    std::cout << str2 << std::endl;
    std::cout << "===============================================" << std::endl;
}

//加载配置文件
bool loadConfig(CONFIG & config,std::string jsonPath){
    std::ifstream ifs(jsonPath);
    if (!ifs.is_open()) {
	logout("1");
        return false;
    }
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(ifs, root, false)) {
	logout("2");
        return false;
    }
	//kafka配置
	config.kafkaconfig.enable = root["KafkaConfig"]["enable"].asBool();
	config.kafkaconfig.ip = root["KafkaConfig"]["ip"].asString();
	config.kafkaconfig.port = root["KafkaConfig"]["port"].asInt();
	config.kafkaconfig.topic = root["KafkaConfig"]["topic"].asString();
	config.kafkaconfig.user = root["KafkaConfig"]["user"].asString();
	config.kafkaconfig.password = root["KafkaConfig"]["password"].asString();
	//相机配置
	config.rtspList.clear();
    for (int i = 0; i < root["CamereConfig"]["cameraList"].size(); i++) {
		RTSPCONFIG rtspconfig;
		rtspconfig.camID = root["CamereConfig"]["cameraList"][i]["camID"].asString();
		rtspconfig.rtspPath = root["CamereConfig"]["cameraList"][i]["rtspPath"].asString();
		rtspconfig.width = root["CamereConfig"]["cameraList"][i]["width"].asInt();
		rtspconfig.height = root["CamereConfig"]["cameraList"][i]["height"].asInt();
		config.rtspList.push_back(rtspconfig);
    }
	//标签配置
	config.labelconfig.class_num = root["LabelConfig"]["labelList"].size();
	config.labelconfig.labelList.clear();
    for (int i = 0; i < root["LabelConfig"]["labelList"].size(); i++) {
		std::string label = root["LabelConfig"]["labelList"][i].asString();
		config.labelconfig.labelList.push_back(label);
    }
	return true;
}

void printConfig(CONFIG & config){
    std::cout << "==============================================="<<std::endl;
    std::cout << "		  CONFIG " << std::endl;
    std::cout << "==============================================="<<std::endl;
	std::cout<<"config.kafkaconfig.ip: "<<config.kafkaconfig.ip<<std::endl;
	std::cout<<"config.kafkaconfig.port: "<<config.kafkaconfig.port<<std::endl;
	std::cout<<"config.kafkaconfig.topic: "<<config.kafkaconfig.topic<<std::endl;
	std::cout<<"config.kafkaconfig.user: "<<config.kafkaconfig.user<<std::endl;
	std::cout<<"config.kafkaconfig.password: "<<config.kafkaconfig.password<<std::endl;
    std::cout << "===============================================" << std::endl;
	std::cout<<"camera_num: "<<config.rtspList.size()<<std::endl;
    for (int i = 0; i < config.rtspList.size(); i++) {
		std::cout<<i<<": "<<config.rtspList[i].camID<<": "<<config.rtspList[i].rtspPath<<std::endl;
    }
    std::cout << "===============================================" << std::endl;
	std::cout<<"class_num: "<<config.labelconfig.class_num<<std::endl;
	std::cout<<"labelList: ";
	for(int i = 0;i<config.labelconfig.labelList.size();i++){
		std::cout<<config.labelconfig.labelList[i]<<" ";
	}
	std::cout<<std::endl;
    std::cout << "==============================================="<<std::endl;
}
bool loadConfig(CONFIG & config){
	return loadConfig(config,"/home/nvidia/project/jetsonDetect/config/config.json");
}
