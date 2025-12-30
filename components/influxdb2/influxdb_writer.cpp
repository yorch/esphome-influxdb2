#include "influxdb_writer.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <string>
#include <set>

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {
namespace influxdb {
static const char *TAG = "influxdb_jab";

void InfluxDBWriter::setup() {
  ESP_LOGCONFIG(TAG, "Setting up InfluxDB Writer...");
  std::vector<EntityBase *> objs;
  for (auto fun : setup_callbacks)
    objs.push_back(fun());

  this->service_url = "http://" + this->host + ":" + to_string(this->port) +
                      "/api/v2/write?org=" + this->orgid + "&bucket=" + this->bucket + "&precision=ns";

  // No need to create a new HttpRequestComponent - it's set via set_http_request()
  // Just prepare the headers for later use
  this->headers_.clear();
  http_request::Header header;
  header.name = "Content-Type";
  header.value = "text/plain";
  this->headers_.push_back(header);
  if ((this->orgid.length() > 0) && (this->token.length() > 0)) {
    header.name = "Authorization";
    header.value = this->token.c_str();
    this->headers_.push_back(header);
  }

  if (publish_all) {
#ifdef USE_BINARY_SENSOR
    for (auto *obj : App.get_binary_sensors()) {
      if (!obj->is_internal() &&
          std::none_of(objs.begin(), objs.end(),
                       [&obj](EntityBase *o) { return o == obj; }))
        obj->add_on_state_callback([this, obj](bool state) {
          this->on_sensor_update(obj, obj->get_object_id(), tags, "", state);
        });
    }
#endif
#ifdef USE_SENSOR
    for (auto *obj : App.get_sensors()) {
      if (!obj->is_internal() &&
          std::none_of(objs.begin(), objs.end(),
                       [&obj](EntityBase *o) { return o == obj; }))
        obj->add_on_state_callback([this, obj](float state) {
          this->on_sensor_update(obj, obj->get_object_id(), tags, "", state);
        });
    }
#endif
#ifdef USE_TEXT_SENSOR
    for (auto *obj : App.get_text_sensors()) {
      if (!obj->is_internal() &&
          std::none_of(objs.begin(), objs.end(),
                       [&obj](EntityBase *o) { return o == obj; }))
        obj->add_on_state_callback([this, obj](std::string state) {
          this->on_sensor_update(obj, obj->get_object_id(), tags, "", state);
        });
    }
#endif
  }
}

void InfluxDBWriter::loop() {}

void InfluxDBWriter::write(std::string measurement,
                           /* unused */ std::string tags,
                           const std::string value, std::string retention,
                           bool is_string) {
  if (this->request_ == nullptr) {
    ESP_LOGW(TAG, "HttpRequestComponent not set, cannot write to InfluxDB");
    return;
  }

  std::replace(measurement.begin(), measurement.end(), '-', '_');
  std::string line =
      measurement + ",device=" + this->device + " value=" + (is_string ? ("\"" + value + "\"") : value);
  
  std::string url = this->service_url +
      (retention.empty() ? "" : "&rp=" + retention + "&precision=s");

  // Use the new perform() method with POST
  // collect_headers is required by the API but we don't need response headers for InfluxDB writes
  std::string method = "POST";
  std::set<std::string> collect_headers;
  auto response = this->request_->perform(url, method, line, this->headers_, collect_headers);

  ESP_LOGD(TAG, "InfluxDB packet: %s", line.c_str());
  
  if (response) {
    ESP_LOGD(TAG, "InfluxDB response status: %d", response->status_code);
  }
}

void InfluxDBWriter::dump_config() {
  ESP_LOGCONFIG(TAG, "InfluxDB Writer:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", host.c_str(), port);
  ESP_LOGCONFIG(TAG, "  Bucket: %s", bucket.c_str());
}

#ifdef USE_BINARY_SENSOR
void InfluxDBWriter::on_sensor_update(binary_sensor::BinarySensor *obj,
                                      std::string measurement, std::string tags,
                                      std::string retention, bool state) {
  write(measurement, tags, state ? "t" : "f", retention, false);
}
#endif

#ifdef USE_SENSOR
void InfluxDBWriter::on_sensor_update(sensor::Sensor *obj,
                                      std::string measurement, std::string tags,
                                      std::string retention, float state) {
  if (!isnan(state))
    write(measurement, tags, to_string(state), retention, false);
}
#endif

#ifdef USE_TEXT_SENSOR
void InfluxDBWriter::on_sensor_update(text_sensor::TextSensor *obj,
                                      std::string measurement, std::string tags,
                                      std::string retention,
                                      std::string state) {
  write(measurement, tags, state, retention, true);
}
#endif

} // namespace influxdb
} // namespace esphome
