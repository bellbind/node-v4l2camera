#include "capture.h"
#include <node.h>
#include <v8.h>
#include <uv.h>

#include <errno.h>
#include <string.h>

#include <string>
#include <sstream>


namespace {
  struct CallbackData {
    v8::Persistent<v8::Object> thisObj;
    v8::Persistent<v8::Function> callback;
  };
  
  class Camera : node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> exports);
  private:
    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Start(const v8::Arguments& args);
    static v8::Handle<v8::Value> Stop(const v8::Arguments& args);
    static v8::Handle<v8::Value> Capture(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToYUYV(const v8::Arguments& args);
    static v8::Handle<v8::Value> ToRGB(const v8::Arguments& args);
    static v8::Handle<v8::Value> ConfigGet(const v8::Arguments& args);
    static v8::Handle<v8::Value> ConfigSet(const v8::Arguments& args);
    static v8::Handle<v8::Value> ControlGet(const v8::Arguments& args);
    static v8::Handle<v8::Value> ControlSet(const v8::Arguments& args);
    
    static v8::Local<v8::Object> Controls(camera_t* camera);  
    static v8::Local<v8::Object> Formats(camera_t* camera);  
    static void StopCB(uv_poll_t* handle, int status, int events);
    static void CaptureCB(uv_poll_t* handle, int status, int events);
    
    static void 
    WatchCB(uv_poll_t* handle, void (*callbackCall)(CallbackData* data));
    static v8::Handle<v8::Value> 
    Watch(const v8::Arguments& args, uv_poll_cb cb);
    Camera();
    ~Camera();
    camera_t* camera;
  };
  
  //[error message handling]
  struct LogContext {
    std::string msg;
  };
  static void logRecord(camera_log_t type, const char* msg, void* pointer) {
    std::stringstream ss;
    switch (type) {
    case CAMERA_ERROR:
      ss << "CAMERA ERROR [" << msg << "] " << errno << " " << strerror(errno);
      break;
    case CAMERA_FAIL:
      ss << "CAMERA FAIL [" << msg << "]";
      break;
    case CAMERA_INFO:
      ss << "CAMERA INFO [" << msg << "]";
      break;
    }
    static_cast<LogContext*>(pointer)->msg = ss.str();
  }
  
  static inline v8::Handle<v8::Value> throwTypeError(const char* msg) {
    auto message = v8::String::New(msg);
    return v8::ThrowException(v8::Exception::TypeError(message));
  }
  static inline v8::Handle<v8::Value> throwError(const char* msg) {
    auto message = v8::String::New(msg);
    return v8::ThrowException(v8::Exception::Error(message));
  }
  static inline v8::Handle<v8::Value> throwError(camera_t* camera) {
    auto ctx = static_cast<LogContext*>(camera->context.pointer);
    return throwError(ctx->msg.c_str());
  }
  
  //[helpers]
  static inline v8::Local<v8::Value>
  getValue(const v8::Local<v8::Object>& self, const char* name) {
    return self->Get(v8::String::NewSymbol(name));
  }
  static inline int32_t
  getInt(const v8::Local<v8::Object>& self, const char* name) {
    return getValue(self, name)->Int32Value();
  }
  static inline uint32_t
  getUint(const v8::Local<v8::Object>& self, const char* name) {
    return getValue(self, name)->Uint32Value();
  }
  
  static inline void 
  setValue(const v8::Local<v8::Object>& self, const char* name, 
           const v8::Handle<v8::Value>& value) {
    self->Set(v8::String::NewSymbol(name), value);
  }
  static inline void 
  setInt(const v8::Local<v8::Object>& self, const char* name, int32_t value) {
    setValue(self, name, v8::Integer::New(value));
  }
  static inline void 
  setUint(const v8::Local<v8::Object>& self, const char* name, 
          uint32_t value) {
    setValue(self, name, v8::Integer::NewFromUnsigned(value));
  }
  static inline void 
  setString(const v8::Local<v8::Object>& self, const char* name, 
            const char* value) {
    setValue(self, name, v8::String::New(value));
  }
  static inline void 
  setBool(const v8::Local<v8::Object>& self, const char* name, bool value) {
    setValue(self, name, v8::Boolean::New(value));
  }

  //[callback helpers]
  void Camera::WatchCB(uv_poll_t* handle, 
                       void (*callbackCall)(CallbackData* data)) {
    v8::HandleScope scope;
    auto data = static_cast<CallbackData*>(handle->data);
    uv_poll_stop(handle);
    uv_close(reinterpret_cast<uv_handle_t*>(handle), 
             [](uv_handle_t* handle) -> void {delete handle;});
    callbackCall(data);
    data->thisObj.Dispose();
    data->callback.Dispose();
    delete data;
  }
  v8::Handle<v8::Value> 
  Camera::Watch(const v8::Arguments& args, uv_poll_cb cb) {
    v8::HandleScope scope;
    auto data = new CallbackData;
    auto thisObj = args.This();
    data->thisObj = v8::Persistent<v8::Object>::New(thisObj);
    data->callback = 
      v8::Persistent<v8::Function>::New(args[0].As<v8::Function>());
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    
    uv_poll_t* handle = new uv_poll_t;
    handle->data = data;
    uv_poll_init(uv_default_loop(), handle, camera->fd);
    uv_poll_start(handle, UV_READABLE, cb);
    return v8::Undefined();
  }
  
  //[methods]
  static const char* control_type_names[] = {
    "invalid",
    "int",
    "bool",
    "menu",
    "int64",
    "class"
    "string",
    "bitmask",
    "int_menu",
  };
  v8::Local<v8::Object> Camera::Controls(camera_t* camera) {
    auto ccontrols = camera_controls_new(camera);
    auto controls = v8::Array::New(ccontrols->length);
    for (size_t i = 0; i < ccontrols->length; i++) {
      auto ccontrol = &ccontrols->head[i];
      auto control = v8::Object::New();
      auto name = v8::String::New(reinterpret_cast<char*>(ccontrol->name));
      controls->Set(i, control);
      controls->Set(name, control);
      setUint(control, "id", ccontrol->id);
      setValue(control, "name", name);
      setString(control, "type", control_type_names[ccontrol->type]);
      setInt(control, "min", ccontrol->min);
      setInt(control, "max", ccontrol->max);
      setInt(control, "step", ccontrol->step);
      setInt(control, "default", ccontrol->default_value);
      auto flags = v8::Object::New();
      setValue(control, "flags", flags);
      setBool(flags, "disabled", ccontrol->flags.disabled);
      setBool(flags, "grabbed", ccontrol->flags.grabbed);
      setBool(flags, "readOnly", ccontrol->flags.read_only);
      setBool(flags, "update", ccontrol->flags.update);
      setBool(flags, "inactive", ccontrol->flags.inactive);
      setBool(flags, "slider", ccontrol->flags.slider);
      setBool(flags, "writeOnly", ccontrol->flags.write_only);
      setBool(flags, "volatile", ccontrol->flags.volatile_value);
      auto menu = v8::Array::New(ccontrol->menus.length);
      setValue(control, "menu", menu);
      switch (ccontrol->type) {
      case CAMERA_CTRL_MENU:
        for (size_t j = 0; j < ccontrol->menus.length; j++) {
          auto value = reinterpret_cast<char*>(ccontrol->menus.head[j].name);
          menu->Set(j, v8::String::New(value));
        }
        break;
#ifndef CAMERA_OLD_VIDEODEV2_H
      case CAMERA_CTRL_INTEGER_MENU:
        for (size_t j = 0; j < ccontrol->menus.length; j++) {
          auto value = static_cast<int32_t>(ccontrol->menus.head[j].value);
          menu->Set(j, v8::Integer::New(value));
        }
        break;
#endif
      default: break;
      }
    }
    camera_controls_delete(ccontrols);
    return controls;
  }
  
  static v8::Local<v8::Object> convertFormat(camera_format_t* cformat) {
    char name[5];
    camera_format_name(cformat->format, name);
    auto format = v8::Object::New();
    setString(format, "formatName", name);
    setUint(format, "format", cformat->format);
    setUint(format, "width", cformat->width);
    setUint(format, "height", cformat->height);
    auto interval = v8::Object::New();
    setValue(format, "interval", interval);
    setUint(interval, "numerator", cformat->interval.numerator);
    setUint(interval, "denominator", cformat->interval.denominator);
    return format;
  }
  v8::Local<v8::Object> Camera::Formats(camera_t* camera) {
    auto cformats = camera_formats_new(camera);
    auto formats = v8::Array::New(cformats->length);
    for (size_t i = 0; i < cformats->length; i++) {
      auto cformat = &cformats->head[i];
      auto format = convertFormat(cformat);
      formats->Set(i, format);
    }
    return formats;
  }
  
  Camera::Camera() : camera(nullptr) {}
  Camera::~Camera() {
    if (camera) {
      auto ctx = static_cast<LogContext*>(camera->context.pointer);
      camera_close(camera);
      delete ctx;
    }
  }
  
  //[members]
  v8::Handle<v8::Value> Camera::New(const v8::Arguments& args) {
    v8::HandleScope scope;
    if (args.Length() < 1) return throwTypeError("argument required: device");
    v8::String::Utf8Value device(args[0]->ToString());
    auto camera = camera_open(*device);
    if (!camera) return throwError(strerror(errno));
    camera->context.pointer = new LogContext;
    camera->context.log = &logRecord;
    
    auto thisObj = args.This();
    auto self = new Camera();
    self->camera = camera;
    self->Wrap(thisObj);
    setValue(thisObj, "device", args[0]);
    setValue(thisObj, "formats", Formats(camera));
    setValue(thisObj, "controls", Controls(camera));
    return scope.Close(thisObj);
  }

  v8::Handle<v8::Value> Camera::Start(const v8::Arguments& args) {
    v8::HandleScope scope;
    auto thisObj = args.This();
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    if (!camera_start(camera)) return throwError(camera);
    setUint(thisObj, "width", camera->width);
    setUint(thisObj, "height", camera->height);
    return scope.Close(thisObj);
  }
  
  
  void Camera::StopCB(uv_poll_t* handle, int /*status*/, int /*events*/) {
    auto callCallback = [](CallbackData* data) -> void {
      v8::HandleScope scope;
      auto thisObj = v8::Local<v8::Object>::New(data->thisObj);
      data->callback->Call(thisObj, 0, nullptr);
    };
    WatchCB(handle, callCallback);
  }
  v8::Handle<v8::Value> Camera::Stop(const v8::Arguments& args) {
    v8::HandleScope scope;
    auto thisObj = args.This();
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    if (!camera_stop(camera)) return throwError(camera);
    return Watch(args, StopCB);
  }
  
  void Camera::CaptureCB(uv_poll_t* handle, int /*status*/, int /*events*/) {
    auto callCallback = [](CallbackData* data) -> void {
      v8::HandleScope scope;
      auto thisObj = v8::Local<v8::Object>::New(data->thisObj);
      auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
      bool captured = camera_capture(camera);
      v8::Local<v8::Value> argv[] = {
        v8::Local<v8::Value>::New(v8::Boolean::New(captured)),
      };
      data->callback->Call(thisObj, 1, argv);
    };
    WatchCB(handle, callCallback);
  }
  v8::Handle<v8::Value> Camera::Capture(const v8::Arguments& args) {
    return Watch(args, CaptureCB);
  }
  
  v8::Handle<v8::Value> Camera::ToYUYV(const v8::Arguments& args) {
    v8::HandleScope scope;
    auto thisObj = args.This();
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    int size = camera->width * camera->height * 2;
    auto ret  = v8::Array::New(size);
    for (int i = 0; i < size; i++) {
      ret->Set(i, v8::Integer::NewFromUnsigned(camera->head.start[i]));
    }
    return scope.Close(ret);
  }
  
  v8::Handle<v8::Value> Camera::ToRGB(const v8::Arguments& args) {
    v8::HandleScope scope;
    auto thisObj = args.This();
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    auto rgb = yuyv2rgb(camera->head.start, camera->width, camera->height);
    int size = camera->width * camera->height * 3;
    auto ret  = v8::Array::New(size);
    for (int i = 0; i < size; i++) {
      ret->Set(i, v8::Integer::NewFromUnsigned(rgb[i]));
    }
    free(rgb);
    return scope.Close(ret);
  }

  v8::Handle<v8::Value> Camera::ConfigGet(const v8::Arguments& args) {
    v8::HandleScope scope;
    auto thisObj = args.This();
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    camera_format_t cformat;
    if (!camera_config_get(camera, &cformat)) return throwError(camera);
    auto format = convertFormat(&cformat);
    return scope.Close(format);
  }
  
  v8::Handle<v8::Value> Camera::ConfigSet(const v8::Arguments& args) {
    v8::HandleScope scope;
    if (args.Length() < 1) throwTypeError("argument required: config");
    auto format = args[0]->ToObject();
    uint32_t width = getUint(format, "width");
    uint32_t height = getUint(format, "height");
    uint32_t numerator = 0;
    uint32_t denominator = 0;
    auto finterval = getValue(format, "interval");
    if (finterval->IsObject()) {
      auto interval = finterval->ToObject();
      numerator = getUint(interval, "numerator");
      denominator = getUint(interval, "denominator");
    }
    camera_format_t cformat = {0, width, height, {numerator, denominator}};
    auto thisObj = args.This();
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    if (!camera_config_set(camera, &cformat)) return throwError(camera);
    setUint(thisObj, "width", camera->width);
    setUint(thisObj, "height", camera->height);
    return scope.Close(thisObj);
  }
  
  v8::Handle<v8::Value> Camera::ControlGet(const v8::Arguments& args) {
    v8::HandleScope scope;
    if (args.Length() < 1) return throwTypeError("an argument required: id");
    uint32_t id = args[0]->Uint32Value();
    auto thisObj = args.This();
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    int32_t value = 0;
    bool success = camera_control_get(camera, id, &value);
    if (!success) return throwError(camera);
    return scope.Close(v8::Integer::New(value));
  }
  
  v8::Handle<v8::Value> Camera::ControlSet(const v8::Arguments& args) {
    v8::HandleScope scope;
    if (args.Length() < 2) 
      return throwTypeError("arguments required: id, value");
    uint32_t id = args[0]->Uint32Value();
    int32_t value = args[1]->Int32Value();
    auto thisObj = args.This();
    auto camera = node::ObjectWrap::Unwrap<Camera>(thisObj)->camera;
    bool success = camera_control_set(camera, id, value);
    if (!success) return throwError(camera);
    return scope.Close(thisObj);
  }
  
  
  //[module init]
  static inline void 
  setMethod(const v8::Local<v8::ObjectTemplate>& proto, const char* name, 
            v8::Handle<v8::Value> (*func)(const v8::Arguments& args)) {
    auto funcValue = v8::FunctionTemplate::New(func)->GetFunction();
    proto->Set(v8::String::NewSymbol(name), funcValue);
  }
  void Camera::Init(v8::Handle<v8::Object> exports) {
    v8::HandleScope scope;
    auto name = v8::String::NewSymbol("Camera");
    auto clazz = v8::FunctionTemplate::New(New);
    clazz->SetClassName(name);
    clazz->InstanceTemplate()->SetInternalFieldCount(1);
    auto proto = clazz->PrototypeTemplate();
    setMethod(proto, "start", Start);
    setMethod(proto, "stop", Stop);
    setMethod(proto, "capture", Capture);
    setMethod(proto, "toYUYV", ToYUYV);
    setMethod(proto, "toRGB", ToRGB);
    setMethod(proto, "configGet", ConfigGet);
    setMethod(proto, "configSet", ConfigSet);
    setMethod(proto, "controlGet", ControlGet);
    setMethod(proto, "controlSet", ControlSet);
    auto ctor = v8::Local<v8::Function>::New(clazz->GetFunction());
    exports->Set(name, ctor);
  }
}
NODE_MODULE(v4l2camera, Camera::Init)
