#include <jetson-utils/videoSource.h>
#include <jetson-utils/videoOutput.h>
#include<opencv2/opencv.hpp>
#include<iostream>
#include<fstream>
#include<vector>
#include<json/json.h>
#include<unistd.h>
#include<thread>
#include<mutex>
#include<time.h>
#include <sys/time.h>

#include "rdkafka.h"

#include "config.h"
#include "cuda_utils.h"
#include "logging.h"
#include "utils.h"
#include "preprocess.h"
#include "postprocess.h"
#include "model.h"

using namespace nvinfer1;

typedef struct {
	std::string camid;//相机ID
	cv::Mat frame;//一帧图像
}FrameSet;

//创建图像容器
std::vector<FrameSet> frameList;
//线程和线程锁
std::mutex mtx;

static Logger gLogger;
const static int kOutputSize = kMaxNumOutputBbox * sizeof(Detection) / sizeof(float) + 1;

cv::Mat uchar3ToCvMat(uchar3* data, int width, int height) {
    cv::Mat cv_mat = cv::Mat(height, width, CV_8UC3);
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            cv_mat.at<cv::Vec3b>(i, j) = cv::Vec3b(data[i * width + j].z, data[i * width + j].y, data[i * width + j].x);
        }
    }
    return cv_mat;
}

void prepare_buffers(ICudaEngine* engine, float** gpu_input_buffer, float** gpu_output_buffer, float** cpu_output_buffer) {
  assert(engine->getNbBindings() == 2);
  // In order to bind the buffers, we need to know the names of the input and output tensors.
  // Note that indices are guaranteed to be less than IEngine::getNbBindings()
  const int inputIndex = engine->getBindingIndex(kInputTensorName);
  const int outputIndex = engine->getBindingIndex(kOutputTensorName);
  assert(inputIndex == 0);
  assert(outputIndex == 1);
  // Create GPU buffers on device
  CUDA_CHECK(cudaMalloc((void**)gpu_input_buffer, kBatchSize * 3 * kInputH * kInputW * sizeof(float)));
  CUDA_CHECK(cudaMalloc((void**)gpu_output_buffer, kBatchSize * kOutputSize * sizeof(float)));

  *cpu_output_buffer = new float[kBatchSize * kOutputSize];
}

void infer(IExecutionContext& context, cudaStream_t& stream, void** gpu_buffers, float* output, int batchsize) {
  context.enqueue(batchsize, gpu_buffers, stream, nullptr);
  CUDA_CHECK(cudaMemcpyAsync(output, gpu_buffers[1], batchsize * kOutputSize * sizeof(float), cudaMemcpyDeviceToHost, stream));
  cudaStreamSynchronize(stream);
}

void serialize_engine(unsigned int max_batchsize, bool& is_p6, float& gd, float& gw, std::string& wts_name, std::string& engine_name) {
  // Create builder
  IBuilder* builder = createInferBuilder(gLogger);
  IBuilderConfig* config = builder->createBuilderConfig();

  // Create model to populate the network, then set the outputs and create an engine
  ICudaEngine *engine = nullptr;
  if (is_p6) {
    engine = build_det_p6_engine(max_batchsize, builder, config, DataType::kFLOAT, gd, gw, wts_name);
  } else {
    engine = build_det_engine(max_batchsize, builder, config, DataType::kFLOAT, gd, gw, wts_name);
  }
  assert(engine != nullptr);

  // Serialize the engine
  IHostMemory* serialized_engine = engine->serialize();
  assert(serialized_engine != nullptr);

  // Save engine to file
  std::ofstream p(engine_name, std::ios::binary);
  if (!p) {
    std::cerr << "Could not open plan output file" << std::endl;
    assert(false);
  }
  p.write(reinterpret_cast<const char*>(serialized_engine->data()), serialized_engine->size());

  // Close everything down
  engine->destroy();
  config->destroy();
  serialized_engine->destroy();
  builder->destroy();
}

void deserialize_engine(std::string& engine_name, IRuntime** runtime, ICudaEngine** engine, IExecutionContext** context) {
  std::ifstream file(engine_name, std::ios::binary);
#include "rdkafka.h"
  if (!file.good()) {
    std::cerr << "read " << engine_name << " error!" << std::endl;
    assert(false);
  }
  size_t size = 0;
  file.seekg(0, file.end);
  size = file.tellg();
  file.seekg(0, file.beg);
  char* serialized_engine = new char[size];
  assert(serialized_engine);
  file.read(serialized_engine, size);
  file.close();

  *runtime = createInferRuntime(gLogger);
  assert(*runtime);
  *engine = (*runtime)->deserializeCudaEngine(serialized_engine, size);
  assert(*engine);
  *context = (*engine)->createExecutionContext();
  assert(*context);
  delete[] serialized_engine;
}

void getframe(std::string camid,std::string rtsp_url,int width,int height){
	videoOptions  options;
	options.resource.protocol = "rtsp";
	std::cout<<"getfrmae"<<std::endl;

	videoSource* input = videoSource::Create(rtsp_url.c_str(), options);
	std::cout<<"getframe2"<<std::endl;
	int status = 0;
	int saveindex = 0;
	uchar3 * data;
	while(true){
		//std::cout<<"1"<<std::endl;
		if(!input) std::cout<<"NULL"<<std::endl;
		if(!input->Capture(&data, 5000000, &status)){
			//std::cout<<"1.1"<<std::endl;
				if( status == videoSource::TIMEOUT ){
			//std::cout<<"1.2"<<std::endl;
					std::cout<<"continue"<<std::endl;
					continue;
				}
				std::cout<<camid<<":  eos"<<std::endl;
		}	

		//std::cout<<"2"<<std::endl;
		if(saveindex++%1==0){
			//std::cout<<width<<" "<<height<<std::endl;
			cv::Mat frame = uchar3ToCvMat(data,width,height);
			// cv::Mat frame = uchar3ToCvMat(data,1024,768);
			FrameSet frameset;
			frameset.camid = camid;
			frameset.frame = frame.clone();
			mtx.lock();
			if(frameList.size()<50)
				frameList.push_back(frameset);
			mtx.unlock();
		}
	}
	SAFE_DELETE(input);
}

/* tiler_sink_pad_buffer_probe  will extract metadata received on OSD sink pad
 * and update params for drawing rectangle, object information etc. */
static std::string base64Decode(const char* Data, int DataByte) {
	//解码表
	const char DecodeTable[] =
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		62, // '+'
		0, 0, 0,
		63, // '/'
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // '0'-'9'
		0, 0, 0, 0, 0, 0, 0,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, // 'A'-'Z'
		0, 0, 0, 0, 0, 0,
		26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
		39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, // 'a'-'z'
	};
	std::string strDecode;
	int nValue;
	int i = 0;
	while (i < DataByte) {
		if (*Data != '\r' && *Data != '\n') {
			nValue = DecodeTable[*Data++] << 18;
			nValue += DecodeTable[*Data++] << 12;
			strDecode += (nValue & 0x00FF0000) >> 16;
			if (*Data != '=') {
				nValue += DecodeTable[*Data++] << 6;
				strDecode += (nValue & 0x0000FF00) >> 8;
				if (*Data != '=') {
					nValue += DecodeTable[*Data++];
					strDecode += nValue & 0x000000FF;
				}
			}
			i += 4;
		}
		else {
			Data++;

			i++;
		}
	}
	return strDecode;
}


static std::string base64Encode(const unsigned char* Data, int DataByte) {
	//编码表
	const char EncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	//返回值
	std::string strEncode;
	unsigned char Tmp[4] = { 0 };
	int LineLength = 0;
	for (int i = 0; i < (int)(DataByte / 3); i++) {
		Tmp[1] = *Data++;
		Tmp[2] = *Data++;
		Tmp[3] = *Data++;
		strEncode += EncodeTable[Tmp[1] >> 2];
		strEncode += EncodeTable[((Tmp[1] << 4) | (Tmp[2] >> 4)) & 0x3F];
		strEncode += EncodeTable[((Tmp[2] << 2) | (Tmp[3] >> 6)) & 0x3F];
		strEncode += EncodeTable[Tmp[3] & 0x3F];
		if (LineLength += 4, LineLength == 76) { strEncode += "\r\n"; LineLength = 0; }
	}
	//对剩余数据进行编码
	int Mod = DataByte % 3;
	if (Mod == 1) {
		Tmp[1] = *Data++;
		strEncode += EncodeTable[(Tmp[1] & 0xFC) >> 2];
		strEncode += EncodeTable[((Tmp[1] & 0x03) << 4)];
		strEncode += "==";
	}
	else if (Mod == 2) {
		Tmp[1] = *Data++;
		Tmp[2] = *Data++;
		strEncode += EncodeTable[(Tmp[1] & 0xFC) >> 2];
		strEncode += EncodeTable[((Tmp[1] & 0x03) << 4) | ((Tmp[2] & 0xF0) >> 4)];
		strEncode += EncodeTable[((Tmp[2] & 0x0F) << 2)];
		strEncode += "=";
	}


	return strEncode;
}


static std::string Mat2Base64(const cv::Mat &img, std::string imgType)
{
    //Mat转base64
    std::string img_data;
    std::vector<uchar> vecImg;
    std::vector<int> vecCompression_params;
    vecCompression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    vecCompression_params.push_back(90);
    imgType = "." + imgType;
    cv::imencode(imgType, img, vecImg, vecCompression_params);
    img_data = base64Encode(vecImg.data(), vecImg.size());
    return img_data;
}

std::string isopen(int w,int h){
	return w*1.0/h>1.2?"OFF":"ON";
}

void detect(std::string brokers,std::string topic,CONFIG config){
	std::cout<<"detect"<<std::endl;
	//cudaSetDevice(0);
  	std::string engine_name = "/home/nvidia/project/jetsonDetect/models/best.engine";
  	bool is_p6 = false;
  	float gd = 0.0f, gw = 0.0f;
	std::cout<<"engine:"<<engine_name<<std::endl;
  	
	IRuntime* runtime = nullptr;
	ICudaEngine* engine = nullptr;
	IExecutionContext* context = nullptr;
	deserialize_engine(engine_name, &runtime, &engine, &context);
	cudaStream_t stream;
	CUDA_CHECK(cudaStreamCreate(&stream));
	
	cuda_preprocess_init(kMaxInputImageSize);
	float* gpu_buffers[2];
	float* cpu_output_buffer = nullptr;
	prepare_buffers(engine, &gpu_buffers[0], &gpu_buffers[1], &cpu_output_buffer);

	//kafka config
	rd_kafka_t *rk;
	rd_kafka_conf_t *conf;


	if(config.kafkaconfig.enable){
		//kafka init
		char errstr[512];      /* librdkafka API error reporting buffer */
		conf = rd_kafka_conf_new();
		if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers.c_str(), errstr,
			sizeof(errstr)) != RD_KAFKA_CONF_OK) {
			std::cout<<"kafka init error!"<<std::endl;
			fprintf(stderr, "%s\n", errstr);
			return;
		}
		rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
		if (!rk) {
			fprintf(stderr, "%% Failed to create new producer of kafka: %s\n",
				errstr);
			return;
		}else{
			std::cout<<"kafka init success"<<std::endl;
		}
	}

	while(true){
		usleep(10);
		bool gotframe = false;
		FrameSet frameset;
		mtx.lock();
		if(frameList.size()){
			std::cout<<"frameList.size():"<<frameList.size()<<std::endl;
			frameset = frameList[0];
			// frameList.clear();
			frameList.erase(frameList.begin());
			gotframe = true;
		}
		if(frameList.size()>16){
			frameList.clear();
		}
		mtx.unlock();
		if(gotframe){
			std::vector<cv::Mat> img_batch;
			img_batch.push_back(frameset.frame);
			// Preprocess
			cuda_batch_preprocess(img_batch, gpu_buffers[0], kInputW, kInputH, stream);
			// Run inference
			infer(*context, stream, (void**)gpu_buffers, cpu_output_buffer, kBatchSize);
			// NMS
			std::vector<std::vector<Detection>> res_batch;
			batch_nms(res_batch, cpu_output_buffer, img_batch.size(), kOutputSize, kConfThresh, kNmsThresh);


			//gettime
			struct timeval tv;
			struct timezone tz;
			struct tm *t;
			gettimeofday(&tv,&tz);
			t = localtime(&tv.tv_sec);
			std::string timestemp = 
				std::to_string(1900+t->tm_year)+'-'+
				std::to_string(1+t->tm_mon)+'-'+
				std::to_string(t->tm_mday)+'|'+
				std::to_string(t->tm_hour)+'-'+
				std::to_string(t->tm_min)+'-'+
				std::to_string(t->tm_sec);


			//send to kafka
			std::string json = "{";
			json+="\"camera-id\":\""+frameset.camid+"\",";
			json+="\"timestemp\":\""+timestemp+"\",";
			json+="\"object\":[";


			for(int i = 0;i<res_batch[0].size();i++){
				cv::Rect r = get_rect(frameset.frame, res_batch[0][i].bbox);
				cv::rectangle(frameset.frame,cv::Rect(r.x,r.y,r.width,r.height),cv::Scalar(0,0,255),2);
				int c = (int)(res_batch[0][i].class_id);
				json+="\""+std::to_string(r.x)+"|"+
					std::to_string(r.y)+"|"+
					std::to_string(r.width)+"|"+
					std::to_string(r.height)+"|"+
					config.labelconfig.labelList[c];
				std::string name = config.labelconfig.labelList[c];
				if(config.labelconfig.labelList[c]=="collector"||config.labelconfig.labelList[c]=="switch")
				{
					std::string open = isopen(r.width,r.height);
					json+="_"+open;
					name+="_"+open;
				}
				cv::putText(frameset.frame,name,
					cv::Point(r.x+3,r.y+18),
					cv::FONT_HERSHEY_COMPLEX,0.7,cv::Scalar(0,0,255),2);

				json+="\"";
				if(i<res_batch[0].size()-1){
					json += ",";
				}
				
			}

			json+="],";

			std::string base64str = Mat2Base64(frameset.frame,"jpg");
			std::cout<<"json:"<<json<<"}"<<std::endl;

			//json+="\"image\":\"image-base64\"";
			json+="\"image\":\""+base64str+"\"";
			json+="}";

			
			if(config.kafkaconfig.enable){
				rd_kafka_producev(
				    rk,
				    RD_KAFKA_V_TOPIC(topic.c_str()),
				    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
				    RD_KAFKA_V_VALUE((void *)json.c_str(), json.length()),
				    RD_KAFKA_V_OPAQUE(NULL),
				    RD_KAFKA_V_END
				);
				rd_kafka_poll(rk, 0);	

			}


			//cv::imwrite("/home/nvidia/project/jetsonDetect/build/saveimage/"+frameset.camid+"_"+getCurrentTimeStr()+"_frame.jpg",frameset.frame);
		}
	}
	// Release stream and buffers
	cudaStreamDestroy(stream);
	CUDA_CHECK(cudaFree(gpu_buffers[0]));
	CUDA_CHECK(cudaFree(gpu_buffers[1]));
	delete[] cpu_output_buffer;
	cuda_preprocess_destroy();
	// Destroy the engine
	context->destroy();
	engine->destroy();
	runtime->destroy();
}

int main()
{
	//加载配置文件
	CONFIG config;
	if(!loadConfig(config)){
		return 0;
	}
	printConfig(config);
	std::vector<std::thread> threadList;

	std::cout<<"m0"<<std::endl;
	//开启检测线程，监听相机发来的图像
	std::string brokers = config.kafkaconfig.ip+":"+std::to_string(config.kafkaconfig.port);
	std::string topic = config.kafkaconfig.topic;
	std::thread detectthread(detect,brokers,topic,config);
	detectthread.detach();
	std::cout<<"m1"<<std::endl;
	for(int i = 0;i<config.rtspList.size();i++){
		std::thread t(getframe,config.rtspList[i].camID,config.rtspList[i].rtspPath,config.rtspList[i].width,config.rtspList[i].height);
		t.detach();
		
	}
	while(true){
		sleep(1000);
	}
	return 0;
}
