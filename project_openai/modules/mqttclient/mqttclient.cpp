#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mosquitto.h>
#include <curl.h>

#include "cJSON.h"
#include"mqttclient.h"

#define MQTT_HOST "127.0.0.1"
#define MQTT_PORT 8883
#define MQTT_KEEPALIVE 60
// 主题和消息，可根据需要修改
#define MQTT_TOPIC_PUSH_HEARTBEAT "command/openAi_Api/request/heartbeat"
#define MQTT_HEARTBEAT_MESSAGE "heartbeat message"
#define MQTT_TOPIC_SUBSCRIBE "command/openAi_Api/request/#"
#define MQTT_QOS 1
#define MQTT_USERNAME "NikJiang"
#define MQTT_PASSWORD "nik520"


#define MQTT_TOPIC_PUSH_CHAT "command/openAi_Api/response/openai_chat"
#define MQTT_TOPIC_PUSH_IMAGE "command/openAi_Api/response/openai_image"
#define MQTT_TOPIC_PUSH_FILE_TO_TXT "command/openAi_Api/response/openai_file_to_txt"
#define MQTT_TOPIC_PUSH_SPEECH_TO_TXT "command/openAi_Api/response/openai_speech_to_txt"
#define MQTT_TOPIC_SPEECH_TO_SPEECH	"command/openAi_Api/response/openai_speech_to_speech"

char api_key[256] = {0};
char g_model[128] = {0};
char g_image_model[128] = {0};

// 动态字符串结构体
typedef struct {
    char *ptr;
    size_t len;
} String;

// 初始化动态字符串
void init_string(String *s) {
    s->len = 0;
    s->ptr = (char*)malloc(s->len+1);
    if (s->ptr == NULL) {
        fprintf(stderr, "malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
}
// 响应数据写入文件的回调函数
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// WriteCallback函数
size_t WriteCallback(void *contents, size_t size, size_t nmemb, String *s) {
    size_t new_len = s->len + size*nmemb;
    s->ptr = (char*)realloc(s->ptr, new_len+1);
    if (s->ptr == NULL) {
        fprintf(stderr, "realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr+s->len, contents, size*nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size*nmemb;
}

// 调用OpenAI API的函数
char* openai_api_chat(const char* api_key, const char* user_content) {
    CURL *curl;
    CURLcode res;
    char *data = NULL;
    char *response = NULL;
    long http_code = 0;

    curl = curl_easy_init();
    if(curl) {
        struct curl_slist *headers = NULL;
        char auth_header[256] = {0};
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);

        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, auth_header);

        // 构造请求数据
        int dataLen = strlen(user_content)+1024;
        data = (char*)malloc(dataLen); // 根据需要调整大小
        snprintf(data, dataLen, "{\"model\": \"%s\", \"messages\": [{\"role\": \"system\", \"content\": \"You are a helpful assistant.\"},{\"role\": \"user\", \"content\": \"%s\"}]}", g_model, user_content);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
       

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if(res != CURLE_OK || http_code != 200) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(data);
    }

    return response; // 需要外部释放这个内存
}

char* openai_text_generate_image(const char* api_key, const char* prompt) {
    CURL *curl;
    CURLcode res;
    String s;
    init_string(&s);

    curl = curl_easy_init();
    if(curl) {
        struct curl_slist *headers = NULL;
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
        
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, auth_header);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/images/generations");
        
        char postfields[1024]; // Make sure this buffer is large enough.
        snprintf(postfields, sizeof(postfields), "{\"model\": \"%s\", \"prompt\": \"%s\", \"n\": 1, \"size\": \"1024x1024\"}", g_image_model, prompt);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return s.ptr;
}

void openai_api_file_to_text(const char *api_key, const char *base64_image, char **buf) {
  CURL *curl;
  CURLcode res;
  String s;
  init_string(&s);

  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  if(curl) {
    struct curl_slist *headers = NULL;
    char *data_template =(char*)"{\"model\": \"%s\", \"messages\": [{\"role\": \"user\", \"content\": [{\"type\": \"text\", \"text\": \"What’s in this image?\"},{\"type\": \"image_url\", \"image_url\": {\"url\": \"data:image/jpeg;base64,%s\"}}]}], \"max_tokens\": 300}";
    char *post_data = (char *)malloc(strlen(base64_image) + strlen(data_template) - 2); // -2 for the %s
    if (!post_data) {
      fprintf(stderr, "Failed to allocate memory for post data\n");
      exit(EXIT_FAILURE);
    }
    sprintf(post_data, data_template, g_model, base64_image);

    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
      *buf = strdup(s.ptr); // Copy the result into buf
    }

    free(s.ptr);
    free(post_data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
}

void openai_audio_to_txt(const char *api_key, const char *file_path, char **resbuf) {
    CURL *curl;
    CURLcode res;
    String s;
    init_string(&s);
    
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        struct curl_httppost *formpost = NULL;
        struct curl_httppost *lastptr = NULL;
        struct curl_slist *headerlist = NULL;
        static const char buf[] = "Expect:";

        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "file",
                     CURLFORM_FILE, file_path,
                     CURLFORM_END);
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "model",
                     CURLFORM_COPYCONTENTS, "whisper-1",
                     CURLFORM_END);
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "response_format",
                     CURLFORM_COPYCONTENTS, "text",
                     CURLFORM_END);
                     
        char auth_header[256] = "Authorization: Bearer ";
        strcat(auth_header, api_key);
        
        headerlist = curl_slist_append(headerlist, buf);
        headerlist = curl_slist_append(headerlist, auth_header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/audio/transcriptions");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        } else {
            *resbuf = strdup(s.ptr); // Copy the result into buf
        }

        free(s.ptr);
        curl_formfree(formpost);
        curl_slist_free_all(headerlist);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}


// 当客户端连接成功到服务器时被调用
void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    if (rc == 0) {
        // 连接成功
        printf("Connected to MQTT server.\n");
        // 订阅主题
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_SUBSCRIBE, MQTT_QOS);
    } else {
        fprintf(stderr, "Failed to connect, return code %d\n", rc);
    }
}

// 假设jsonStr是包含上述JSON数据的字符串
char* parse_content_from_chat_json(const char* jsonStr) {
   cJSON *json = cJSON_Parse(jsonStr);
    if (!json) {
        printf("Error before: [%s]\n", cJSON_GetErrorPtr());
        return NULL;
    }

    cJSON *choices = cJSON_GetObjectItem(json, "choices");
    if (!choices) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *choice = cJSON_GetArrayItem(choices, 0);
    if (!choice) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *message = cJSON_GetObjectItem(choice, "message");
    if (!message) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *content = cJSON_GetObjectItem(message, "content");
    if (!content) {
        cJSON_Delete(json);
        return NULL;
    }

    char *result = strdup(content->valuestring);

    cJSON_Delete(json);

    return result;
}
//步骤1: 解析JSON字符串来获取url的值
// 定义一个结构体来存储解析结果
typedef struct {
    char *revised_prompt;
    char *url;
} ParsedData;

// 解析JSON字符串的函数
ParsedData parse_imageInfojson(const char *json_string) {
    ParsedData result = {0};

    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return result;
    }

    // 访问data数组
    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (data == NULL || !cJSON_IsArray(data)) {
        cJSON_Delete(json);
        return result;
    }

    // 获取data数组的第一个元素
    cJSON *first_item = cJSON_GetArrayItem(data, 0);
    if (first_item == NULL) {
        cJSON_Delete(json);
        return result;
    }

    // 提取revised_prompt和url
    cJSON *revised_prompt = cJSON_GetObjectItemCaseSensitive(first_item, "revised_prompt");
    cJSON *url = cJSON_GetObjectItemCaseSensitive(first_item, "url");
    if (revised_prompt != NULL && cJSON_IsString(revised_prompt) && revised_prompt->valuestring != NULL) {
        result.revised_prompt = strdup(revised_prompt->valuestring);
    }
    if (url != NULL && cJSON_IsString(url) && url->valuestring != NULL) {
        result.url = strdup(url->valuestring);
    }

    cJSON_Delete(json);
    return result;
}

static const char base64_chars[] = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

void base64_encode(const unsigned char *bytes_to_encode, unsigned int in_len, char **encoded_data) {
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];

    int out_len = 4 * ((in_len + 2) / 3); // Base64字符串长度
    *encoded_data = (char*)malloc(out_len + 1); // 分配内存
    if (*encoded_data == NULL) return;

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; (i <4) ; i++)
                (*encoded_data)[j++] = base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            (*encoded_data)[out_len - (i - j)] = base64_chars[char_array_4[j]];

        while((i++ < 3))
            (*encoded_data)[out_len - (3 - i)] = '=';
    }

    (*encoded_data)[out_len] = '\0'; // Null-terminate the string
}

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realSize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char*)realloc(mem->memory, mem->size + realSize + 1);
    if (!ptr) {
        printf("Not enough memory\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realSize);
    mem->size += realSize;
    mem->memory[mem->size] = 0;

    return realSize;
}

int download_and_encode_base64(const char *url, char **buf) {
    CURL *curlHandle;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = (char*)malloc(1);  // will be grown as needed by realloc
    chunk.size = 0;            // no data at this point

    curl_global_init(CURL_GLOBAL_ALL);
    curlHandle = curl_easy_init();
    if (curlHandle) {
        curl_easy_setopt(curlHandle, CURLOPT_URL, url);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curlHandle);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.memory);
            curl_easy_cleanup(curlHandle);
            curl_global_cleanup();
            return -1; // indicate failure
        }

        // Now, chunk.memory points to the downloaded data,
        // and chunk.size is its size. Perform base64 encoding.
        base64_encode((const unsigned char *)chunk.memory, chunk.size, buf);

        // Cleanup
        curl_easy_cleanup(curlHandle);
    }
    curl_global_cleanup();
    if (chunk.memory) {
        free(chunk.memory);
    }

    return 0; // Success
}

//解析api解读文件的json结果
void parse_json_and_get_content(const char *json, char **buf) {
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return;
    }

    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (cJSON_IsArray(choices)) {
        cJSON *choice = cJSON_GetArrayItem(choices, 0);
        if (choice != NULL) {
            cJSON *message = cJSON_GetObjectItemCaseSensitive(choice, "message");
            if (message != NULL) {
                cJSON *content = cJSON_GetObjectItemCaseSensitive(message, "content");
                if (cJSON_IsString(content) && (content->valuestring != NULL)) {
                    *buf = strdup(content->valuestring);
                }
            }
        }
    }

    cJSON_Delete(root);
}

//base64解码
static const char decoding_table[] = {
    62, -1, -1, -1, 63, // + is 62, / is 63, 52-61 are digits
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    -1, -1, -1, -1, -1, -1, -1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, // A-Z
    -1, -1, -1, -1, -1, -1,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 // a-z
};

int base64_decode(const char *base64input, unsigned char **decoded_output, size_t *decoded_length) {
    *decoded_length = strlen(base64input) / 4 * 3;
    if (base64input[strlen(base64input) - 1] == '=') (*decoded_length)--;
    if (base64input[strlen(base64input) - 2] == '=') (*decoded_length)--;

    *decoded_output = (unsigned char *)malloc(*decoded_length);
    if (*decoded_output == NULL) return 0;

    for (int i = 0, j = 0; i < strlen(base64input);) {
        int sextet_a = base64input[i] == '=' ? 0 & i++ : decoding_table[base64input[i++] - 43];
        int sextet_b = base64input[i] == '=' ? 0 & i++ : decoding_table[base64input[i++] - 43];
        int sextet_c = base64input[i] == '=' ? 0 & i++ : decoding_table[base64input[i++] - 43];
        int sextet_d = base64input[i] == '=' ? 0 & i++ : decoding_table[base64input[i++] - 43];

        int triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

        if (j < *decoded_length) (*decoded_output)[j++] = (triple >> 16) & 0xFF;
        if (j < *decoded_length) (*decoded_output)[j++] = (triple >> 8) & 0xFF;
        if (j < *decoded_length) (*decoded_output)[j++] = triple & 0xFF;
    }

    return 1;
}

// 接口函数：将Base64编码的音频数据解码并保存到文件
// 参数：Base64编码的音频数据字符串，目标文件名
// 返回值：0表示成功，非0表示失败
int save_base64_encoded_audio_to_file(const char *base64_audio, const char *filename) {
    unsigned char *decoded_audio = NULL;
    size_t decoded_length = 0;

    // 调用Base64解码函数
    int ret = base64_decode(base64_audio, &decoded_audio, &decoded_length);
    if (decoded_length == (size_t)-1) {
        // 解码失败
        return 1;
    }
	printf("decoded_length=%ld\n", decoded_length);
    // 打开文件准备写入
    FILE *file = fopen(filename, "wb");
    if (!file) {
        // 文件打开失败
        free(decoded_audio);
        return 2;
    }

    // 写入解码后的音频数据到文件
    size_t written = fwrite(decoded_audio, 1, decoded_length, file);
    fclose(file);
    free(decoded_audio);

    // 检查写入是否成功
    if (written != decoded_length) {
        // 写入失败
        return 3;
    }

    return 0; // 成功
}
//去掉字符串换行符
char* remove_newlines(const char* input) {
    if (input == NULL) {
        return NULL;
    }
    
    // 分配足够大的空间以容纳去除换行符后的字符串
    // 实际上可能需要的空间会小于或等于原字符串长度
    char* output = (char*)malloc(strlen(input) + 1);
    if (output == NULL) {
        // 内存分配失败
        return NULL;
    }

    int j = 0;
    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] != '\n') {
            output[j++] = input[i];
        }
    }
    output[j] = '\0'; // 确保新字符串以空字符结尾

    return output;
}

// 读取文件并返回Base64编码的字符串
char *base64_encode_file(const char *filename) {
    FILE *file;
    unsigned char *buffer;
    size_t file_length;
    size_t output_length;

    // 打开文件
    file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Cannot open file %s\n", filename);
        return NULL;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 分配内存
    buffer = (unsigned char *)malloc(file_length);
    if (!buffer) {
        fprintf(stderr, "Memory error\n");
        fclose(file);
        return NULL;
    }

    // 读取文件内容
    fread(buffer, file_length, 1, file);
    fclose(file);

    // Base64编码
    char *encoded_data = NULL;
    base64_encode(buffer, file_length, &encoded_data);
   // base64_encode(const unsigned char *bytes_to_encode, unsigned int in_len, char **encoded_data)
    free(buffer);

    return encoded_data;
}

// 将文本转换为语音并返回Base64编码的字符串
char* text_to_speech_base64(const char* OPENAI_API_KEY, const char* input, char** buf) {
    CURL *curl;
    CURLcode res;
    const char* filename = "speech.wav";

    // 初始化CURL
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        struct curl_slist *headers = NULL;
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", OPENAI_API_KEY);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // 设置CURL选项
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/audio/speech");
        
        // 准备POST数据
        char data[1024];
        snprintf(data, sizeof(data), "{\"model\": \"tts-1\", \"input\": \"%s\", \"voice\": \"shimmer\"}", input);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        
        // 设置写入文件回调
        FILE *fp = fopen(filename, "wb");
        if (fp) {
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            
            // 执行CURL请求
            res = curl_easy_perform(curl);
            fclose(fp);
            
            if(res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                // 对文件进行Base64编码
                *buf = base64_encode_file(filename); // 假设base64_encode_file是一个能够读取文件并返回Base64编码字符串的函数
            }
        } else {
            fprintf(stderr, "Cannot open file %s\n", filename);
        }

        // 清理
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return *buf;
}

/**
 * 删除指定的文件（如果存在），然后重新创建它。
 * 
 * @param filename 要处理的文件名。
 * @return 如果文件成功创建，返回0；如果操作失败，返回-1。
 */
int delete_and_recreate_file(const char* filename) {
    // 尝试删除文件（如果文件存在）
    if (remove(filename) == 0) {
        printf("File '%s' successfully deleted.\n", filename);
    } else {
        // 如果文件不存在，不是错误，但如果其他原因导致删除失败，显示警告
        perror("Warning: Error deleting file or file not exist");
    }
    
    // 尝试创建（或重新创建）文件
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error creating file");
        return -1; // 创建文件失败
    }
    
    printf("File '%s' successfully created.\n", filename);
    fclose(file); // 关闭文件，完成创建过程
    return 0; // 操作成功
}

// 当消息被接收时被调用
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    //printf("Received message: %s from topic: %s\n", (char *)message->payload, message->topic);
    char *resp_buf = NULL;
    // 发布消息
    if(strstr((char *)message->topic, "openai_chat")){
		//访问openai的文本生成api
		if(message->payload){
			char *response = openai_api_chat(api_key, (char *)message->payload);
			if(response) {
			    char* content = parse_content_from_chat_json(response);
			    if (content != NULL) {
			       // printf("Extracted content: %s\n", content);
			        mosquitto_publish(mosq, NULL, MQTT_TOPIC_PUSH_CHAT, strlen(content), content, MQTT_QOS, false);
			        free(content); // 释放分配的内存
			    } else {
			        printf("Could not extract content.\n");
			    }
				free(response); // 清理分配的内存
			}
		}
    }
    else if(strstr((char *)message->topic, "openai_txt_to_image")){
    	
	    char* response = openai_text_generate_image(api_key, (char *)message->payload);
	    
	    ParsedData data = parse_imageInfojson(response);
	    if (data.revised_prompt && data.url) {

		    char *encodedData = NULL;
		
		    if (download_and_encode_base64(data.url, &encodedData) == 0) {
				resp_buf = (char*)malloc(strlen(encodedData) + 512);
				sprintf(resp_buf, "{\"revised_promptt\":\"%s\",\"imagedata\":\"%s\"}", data.revised_prompt,encodedData);
				mosquitto_publish(mosq, NULL, MQTT_TOPIC_PUSH_IMAGE, strlen(resp_buf), resp_buf, MQTT_QOS, false);
				free(resp_buf);
				free(encodedData);
		    } else {
		        printf("Failed to download or encode data.\n");
		    }
	    }
	   // 清理
	    free(data.revised_prompt);
	    free(data.url);
	    
	    free(response);
    }
    else if(strstr((char *)message->topic, "openai_file_to_text"))
    {
		char *json_string = NULL;
		openai_api_file_to_text(api_key, (char *)message->payload, &json_string);
		
		if (json_string) {
			//printf("API Response: %s\n", json_string);
			char *pcontent = NULL;
			
			parse_json_and_get_content(json_string, &pcontent);
			
			if (pcontent) {
			   //printf("Extracted content: %s\n", pcontent);
			   mosquitto_publish(mosq, NULL, MQTT_TOPIC_PUSH_FILE_TO_TXT, strlen(pcontent), pcontent, MQTT_QOS, false);
			   free(pcontent);
			} else {
			   printf("No content found.\n");
			}
			free(json_string);
		}
    }
     else if(strstr((char *)message->topic, "openai_speech_to_txt"))
	{
		printf("file:%s func:%s line:%d\n", __FILE__, __func__, __LINE__);
		char *file_path = (char*)"speech_to_speech.wav";
		if (delete_and_recreate_file(file_path) == 0) {
		   printf("Operation successful.\n");
		} else {
		   printf("Operation failed.\n");
		}
		//1、将接收到的base64音频解码成wav文件
		int ret = save_base64_encoded_audio_to_file((char *)message->payload, file_path);
		if(ret){
			printf("save_base64_encoded_audio_to_file failed!\n");
		}
		//2、调用api接口解读wav文件转为文字
		char *buf = NULL;
		openai_audio_to_txt(api_key, file_path, &buf);
		
		//3、返回识别的语音
		printf("API Response: %s\n", buf);
		mosquitto_publish(mosq, NULL, MQTT_TOPIC_PUSH_SPEECH_TO_TXT, strlen(buf), buf, MQTT_QOS, false);

		//4、去掉换行符
		char* modified_str = remove_newlines(buf);
		
		if (modified_str != NULL) {
		   	printf("Original String:\n%s\n\nModified String:\n%s\n", buf, modified_str);
		  	 //5、调用chat接口对接api
			char *response = openai_api_chat(api_key, modified_str);
			if(response) {
				//printf("response: %s\n", response);
			    char* content = parse_content_from_chat_json(response);
			    if (content != NULL) {
			       // printf("Extracted content: %s\n", content);
			        mosquitto_publish(mosq, NULL, MQTT_TOPIC_PUSH_CHAT, strlen(content), content, MQTT_QOS, false);
			        free(content); // 释放分配的内存
			    } else {
			        printf("Could not extract content.\n");
			    }
				free(response); // 清理分配的内存
			}
		   free(modified_str); // 释放分配的内存
		} else {
		   printf("Memory allocation failed\n");
		}
		
		if (buf) {
		   free(buf);
		}


    }
	else if(strstr((char *)message->topic, "openai_speech_to_speech"))
    {
    		printf("file:%s func:%s line:%d\n", __FILE__, __func__, __LINE__);
    		
		//先删除文件,确保干净
		char *file_path = (char*)"/home/project_openai/build/speech.wav";
		if (delete_and_recreate_file(file_path) == 0) {
		   printf("Operation successful.\n");
		} else {
		   printf("Operation failed.\n");
		}
		//1、将接收到的base64音频解码成wav文件
		int ret = save_base64_encoded_audio_to_file((char *)message->payload, file_path);
		if(ret){
			printf("save_base64_encoded_audio_to_file failed!\n");
		}
		//2、调用api接口解读wav文件转为文字
		char *buf = NULL;
		openai_audio_to_txt(api_key, file_path, &buf);
		
		//3、返回识别的语音文本
		printf("API Response: %s\n", buf);
		mosquitto_publish(mosq, NULL, MQTT_TOPIC_PUSH_SPEECH_TO_TXT, strlen(buf), buf, MQTT_QOS, false);

		//4、去掉换行符
		char* modified_str = remove_newlines(buf);
		if (modified_str != NULL) {
		   //	printf("Original String:\n%s\n\nModified String:\n%s\n", buf, modified_str);
		   //5、调用chat接口对接api,等待openai回答
			char *response = openai_api_chat(api_key, modified_str);
			if(response) {
				//printf("response: %s\n", response);
				//获取文本内容
			    char* content = parse_content_from_chat_json(response);
			    if (content != NULL) {
			       	// printf("Extracted content: %s\n", content);
			       	//返回gpt的生产文本
			       	 mosquitto_publish(mosq, NULL, MQTT_TOPIC_PUSH_CHAT, strlen(content), content, MQTT_QOS, false);
			        	//6、调用asr接口对文本转语音
				  	 char *base64audio = NULL;
				  	 text_to_speech_base64(api_key, content, &base64audio);
				  	 if(base64audio){
				  	 	//返回语音
				  	 	mosquitto_publish(mosq, NULL, MQTT_TOPIC_SPEECH_TO_SPEECH, strlen(base64audio), base64audio, MQTT_QOS, false);
					     free(base64audio); // 释放分配的内存
				  	 }else{
				  	 	printf("text_to_speech_base64 failed\n");
				  	 }
			        free(content); // 释放分配的内存
			    } else {
			        printf("Could not extract content.\n");
			    }
				free(response); // 清理分配的内存
			}
			
		   free(modified_str); // 释放分配的内存
		} else {
		   printf("Memory allocation failed\n");
		}
		
		if (buf) {
		   free(buf);
		}


    }
}

// 发送心跳的间隔时间（秒）
#define HEARTBEAT_INTERVAL 10
void *heartbeat_thread(void *mosq) {
    struct mosquitto *mosquito = (struct mosquitto *)mosq;
    while(1) {
        // 向MQTT主题发送心跳消息
        mosquitto_publish(mosquito, NULL, MQTT_TOPIC_PUSH_HEARTBEAT, strlen(MQTT_HEARTBEAT_MESSAGE), MQTT_HEARTBEAT_MESSAGE, 0, false);
        //printf("Heartbeat message sent\n");

        // 等待一定时间后再次发送
        sleep(HEARTBEAT_INTERVAL);
    }
    return NULL;
}

struct mosquitto *g_mosq;

int MqttClient_Init(char *myapi_key, char *model, char *imagemodel) {
    //struct mosquitto *mosq;
    int rc;
    strcpy(api_key, myapi_key);
    strcpy(g_model, model);
    strcpy(g_image_model, imagemodel);
    // 初始化libmosquitto
    mosquitto_lib_init();

    // 创建MQTT客户端
    g_mosq = mosquitto_new(NULL, true, NULL);
    if (!g_mosq) {
        fprintf(stderr, "Failed to create mosquitto client\n");
        return 1;
    }

    // 设置连接成功和消息接收的回调函数
    mosquitto_connect_callback_set(g_mosq, on_connect);
    mosquitto_message_callback_set(g_mosq, on_message);
	// 设置用户名和密码
    mosquitto_username_pw_set(g_mosq, MQTT_USERNAME, MQTT_PASSWORD);
    
    // 连接到MQTT服务器
    rc = mosquitto_connect(g_mosq, MQTT_HOST, MQTT_PORT, MQTT_KEEPALIVE);
    if (rc) {
        fprintf(stderr, "Could not connect to MQTT server, Error: %s\n", mosquitto_strerror(rc));
        return 1;
    }

    // 启动循环处理网络消息
    rc = mosquitto_loop_start(g_mosq);
    if (rc) {
        fprintf(stderr, "Could not start network loop, Error: %s\n", mosquitto_strerror(rc));
        mosquitto_destroy(g_mosq);
        return 1;
    }
	// 创建心跳包发送线程
    pthread_t tid;
    if(pthread_create(&tid, NULL, heartbeat_thread, (void*)g_mosq)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

	while(1){
	    // 等待一段时间以确保连接和订阅过程完成
	    sleep(1); // 在Windows上使用Sleep
	}
	// 等待心跳线程结束（虽然这个线程实际上是无限循环的）
    pthread_join(tid, NULL);
    // 断开连接并停止消息循环
    mosquitto_disconnect(g_mosq);
    mosquitto_loop_stop(g_mosq, true);

    // 清理
    mosquitto_destroy(g_mosq);
    mosquitto_lib_cleanup();

    return 0;
}

int MqttClient_UnInit()
{
	// 断开连接并停止消息循环
    mosquitto_disconnect(g_mosq);
    mosquitto_loop_stop(g_mosq, true);

    // 清理
    mosquitto_destroy(g_mosq);
    mosquitto_lib_cleanup();

    return 0;
}
