#include "mqtt_manager.hpp"

#include <cstring>
#include <esp_log.h>

static const char* TAG = "MQTT_MANAGER";

static constexpr const char* LWT_TOPIC = "energy_meter/heartbeat";
static constexpr const char* LWT_PAYLOAD = "{\"status\":\"dead\"}";

MqttManager::MqttManager(const ConfigManager& config_manager)
    : _config_manager(config_manager), _client(nullptr), _connected(false) {}

MqttManager::~MqttManager() {
    stop();
}

esp_err_t MqttManager::start() {
    if (_client != nullptr) {
        ESP_LOGW(TAG, "MQTT client already started.");
        return ESP_ERR_INVALID_STATE;
    }

    const AppConfig& config = _config_manager.get_config();
    if (config.mqtt_ip.empty()) {
        ESP_LOGE(TAG, "MQTT IP is not configured. Cannot start MQTT client.");
        return ESP_ERR_INVALID_ARG;
    }

    std::string uri = config.mqtt_ip;
    if (uri.find("://") == std::string::npos) {
        uri = "mqtt://" + uri;
    }

    // Using ESP-IDF v5.x compatible configuration structure
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = uri.c_str();

    if (!config.mqtt_username.empty()) {
        mqtt_cfg.credentials.username = config.mqtt_username.c_str();
    }
    if (!config.mqtt_password.empty()) {
        mqtt_cfg.credentials.authentication.password = config.mqtt_password.c_str();
    }

    mqtt_cfg.session.last_will.topic = LWT_TOPIC;
    mqtt_cfg.session.last_will.msg = LWT_PAYLOAD;
    mqtt_cfg.session.last_will.msg_len = strlen(LWT_PAYLOAD);
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = true;

    ESP_LOGI(TAG, "Initializing MQTT client for URI: %s", uri.c_str());
    _client = esp_mqtt_client_init(&mqtt_cfg);
    if (!_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client.");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, mqtt_event_handler, this);

    esp_err_t err = esp_mqtt_client_start(_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t MqttManager::stop() {
    if (_client == nullptr) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping MQTT client.");
    esp_mqtt_client_stop(_client);
    esp_mqtt_client_destroy(_client);
    _client = nullptr;
    _connected = false;

    return ESP_OK;
}

bool MqttManager::is_connected() const {
    return _connected;
}

int MqttManager::publish(const std::string& topic, const std::string& payload, int qos,
                         bool retain) {
    if (!_connected || !_client) {
        ESP_LOGW(TAG, "Cannot publish, MQTT client not connected.");
        return -1;
    }

    int msg_id = esp_mqtt_client_publish(_client, topic.c_str(), payload.c_str(), payload.length(),
                                         qos, retain);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish message to topic: %s", topic.c_str());
    } else {
        ESP_LOGD(TAG, "Published message to topic: %s, msg_id: %d", topic.c_str(), msg_id);
    }
    return msg_id;
}

int MqttManager::subscribe(const std::string& topic, int qos) {
    if (!_connected || !_client) {
        ESP_LOGW(TAG, "Cannot subscribe, MQTT client not connected.");
        return -1;
    }

    int msg_id = esp_mqtt_client_subscribe(_client, topic.c_str(), qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic.c_str());
    } else {
        ESP_LOGD(TAG, "Subscribed to topic: %s, msg_id: %d", topic.c_str(), msg_id);
    }
    return msg_id;
}

void MqttManager::on_connect(ConnectCallback cb) {
    _connect_cb = std::move(cb);
}

void MqttManager::on_disconnect(DisconnectCallback cb) {
    _disconnect_cb = std::move(cb);
}

void MqttManager::on_message(MessageCallback cb) {
    _message_cb = std::move(cb);
}

void MqttManager::mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id,
                                     void* event_data) {
    MqttManager* manager = static_cast<MqttManager*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    manager->handle_event(event);
}

void MqttManager::handle_event(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        _connected = true;
        if (_connect_cb) {
            _connect_cb();
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        _connected = false;
        if (_disconnect_cb) {
            _disconnect_cb();
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA: {
        ESP_LOGD(TAG, "MQTT_EVENT_DATA");
        if (_message_cb) {
            std::string topic(event->topic, event->topic_len);
            std::string payload(event->data, event->data_len);
            _message_cb(topic, payload);
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGE(TAG, "Reported from tls stack: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGE(TAG, "Captured as transport's socket errno: %d",
                     event->error_handle->esp_transport_sock_errno);
        }
        break;

    default:
        ESP_LOGD(TAG, "Other event id:%d", event->event_id);
        break;
    }
}