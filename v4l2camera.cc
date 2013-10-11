#include "capture.h"
#include <node.h>
#include <v8.h>
#include <uv.h>

#include <errno.h>
#include <string.h>

#include <string>
#include <sstream>

#include <iostream>

namespace {

struct LogContext {
  std::string msg;
};
static void logRecord(camera_log_t type, const char* msg, void* pointer)
{
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

static v8::Handle<v8::Value> throwError(const char* msg)
{
  auto message = v8::String::New(msg);
  return v8::ThrowException(v8::Exception::Error(message));
}

static v8::Handle<v8::Value> throwError(camera_t* camera)
{
  auto ctx = static_cast<LogContext*>(camera->context.pointer);
  return throwError(ctx->msg.c_str());
}

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
static v8::Local<v8::Object> cameraControls(camera_t* camera)
{
  auto ccontrols = camera_controls_new(camera);
  auto controls = v8::Array::New(ccontrols->length);
  for (size_t i = 0; i < ccontrols->length; i++) {
    auto ccontrol = &ccontrols->head[i];
    auto control = v8::Object::New();
    auto name = v8::String::New(reinterpret_cast<char*>(ccontrol->name));
    controls->Set(i, control);
    controls->Set(name, control);
    control->Set(v8::String::NewSymbol("id"), 
                 v8::Integer::NewFromUnsigned(ccontrol->id));
    control->Set(v8::String::NewSymbol("name"), name);
    control->Set(v8::String::NewSymbol("type"), 
                 v8::String::New(control_type_names[ccontrol->type]));
    control->Set(v8::String::NewSymbol("min"), 
                 v8::Integer::NewFromUnsigned(ccontrol->min));
    control->Set(v8::String::NewSymbol("max"), 
                 v8::Integer::NewFromUnsigned(ccontrol->max));
    control->Set(v8::String::NewSymbol("step"), 
                 v8::Integer::NewFromUnsigned(ccontrol->step));
    control->Set(v8::String::NewSymbol("default"), 
                 v8::Integer::NewFromUnsigned(ccontrol->default_value));
    auto flags = v8::Object::New();
    control->Set(v8::String::NewSymbol("flags"), flags);
    flags->Set(v8::String::NewSymbol("disabled"), 
               v8::Boolean::New(ccontrol->flags.disabled));
    flags->Set(v8::String::NewSymbol("grabbed"), 
               v8::Boolean::New(ccontrol->flags.grabbed));
    flags->Set(v8::String::NewSymbol("readOnly"), 
               v8::Boolean::New(ccontrol->flags.read_only));
    flags->Set(v8::String::NewSymbol("update"), 
               v8::Boolean::New(ccontrol->flags.update));
    flags->Set(v8::String::NewSymbol("inactive"), 
               v8::Boolean::New(ccontrol->flags.inactive));
    flags->Set(v8::String::NewSymbol("slider"), 
               v8::Boolean::New(ccontrol->flags.slider));
    flags->Set(v8::String::NewSymbol("writeOnly"), 
               v8::Boolean::New(ccontrol->flags.write_only));
    flags->Set(v8::String::NewSymbol("volatile"), 
               v8::Boolean::New(ccontrol->flags.volatile_value));
    auto menu = v8::Array::New(ccontrol->menus.length);
    control->Set(v8::String::NewSymbol("menu"), menu);
    switch (ccontrol->type) {
    case CAMERA_CTRL_MENU:
      for (size_t j = 0; j < ccontrol->menus.length; j++) {
        auto value = reinterpret_cast<char*>(ccontrol->menus.head[j].name);
        menu->Set(j, v8::String::New(value));
      }
      break;
    case CAMERA_CTRL_INTEGER_MENU:
      for (size_t j = 0; j < ccontrol->menus.length; j++) {
        auto value = static_cast<int32_t>(ccontrol->menus.head[j].value);
        menu->Set(j, v8::Integer::New(value));
      }
      break;
    default: break;
    }
  }
  camera_controls_delete(ccontrols);
  return controls;
}

static void cameraDelete(v8::Persistent<v8::Value> handle, void* param)
{
  auto camera = static_cast<camera_t*>(param);
  auto ctx = static_cast<LogContext*>(camera->context.pointer);
  camera_close(camera);
  delete ctx;
  handle.Dispose();
}
static v8::Handle<v8::Value> cameraNew(const v8::Arguments& args)
{
  v8::HandleScope scope;
  if (args.Length() < 1) {
    auto msg = v8::String::New("argument required: device");
    return v8::ThrowException(v8::Exception::TypeError(msg));
  }
  v8::String::Utf8Value u8device(args[0]->ToString());
  const char* device = *u8device;
  
  auto camera = camera_open(device);
  if (!camera) return throwError(strerror(errno));
  camera->context.pointer = new LogContext;
  camera->context.log = &logRecord;
  auto thisObj = args.This();
  thisObj->SetInternalField(0, v8::External::New(camera));
  auto holder = v8::Persistent<v8::Object>::New(thisObj);
  holder.MakeWeak(camera, cameraDelete);
  thisObj->Set(v8::String::NewSymbol("controls"), cameraControls(camera));
  thisObj->Set(v8::String::NewSymbol("device"), args[0]);
  return scope.Close(thisObj);
}

static v8::Handle<v8::Value> cameraConfig(const v8::Arguments& args)
{
  v8::HandleScope scope;
  if (args.Length() < 1) {
    auto msg = v8::String::New("argument required: config");
    return v8::ThrowException(v8::Exception::TypeError(msg));
  }
  auto config = args[0]->ToObject();
  uint32_t width = config->Get(v8::String::New("width"))->Uint32Value();
  uint32_t height = config->Get(v8::String::New("height"))->Uint32Value();
  uint32_t numerator = 0;
  uint32_t denominator = 0;
  auto pinterval = config->Get(v8::String::New("interval"));
  if (pinterval->IsObject()) {
    auto interval = pinterval->ToObject();
    numerator = interval->Get(v8::String::New("numerator"))->Uint32Value();
    denominator = interval->Get(v8::String::New("denominator"))->Uint32Value();
  }
  camera_config_t cconfig = {0, width, height, {numerator, denominator}};
  auto thisObj = args.This();
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  if (!camera_config(camera, &cconfig)) return throwError(camera);
  thisObj->Set(v8::String::NewSymbol("width"), 
               v8::Integer::NewFromUnsigned(camera->width));
  thisObj->Set(v8::String::NewSymbol("height"), 
               v8::Integer::NewFromUnsigned(camera->height));
  return scope.Close(thisObj);
}

static v8::Handle<v8::Value> cameraStart(const v8::Arguments& args)
{
  v8::HandleScope scope;
  auto thisObj = args.This();
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  if (!camera_start(camera)) return throwError(camera);
  thisObj->Set(v8::String::NewSymbol("width"), 
               v8::Integer::NewFromUnsigned(camera->width));
  thisObj->Set(v8::String::NewSymbol("height"), 
               v8::Integer::NewFromUnsigned(camera->height));
  return scope.Close(thisObj);
}

static v8::Handle<v8::Value> cameraStop(const v8::Arguments& args)
{
  v8::HandleScope scope;
  auto thisObj = args.This();
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  if (!camera_stop(camera)) return throwError(camera);
  return scope.Close(thisObj);
}

struct CaptureData {
  v8::Persistent<v8::Object> thisObj;
  v8::Persistent<v8::Function> callback;
};
static void cameraCaptureClose(uv_handle_t* handle) {
  delete handle;
}
static void cameraCaptureCB(uv_poll_t* handle, int status, int events)
{
  v8::HandleScope scope;
  auto data = static_cast<CaptureData*>(handle->data);
  uv_poll_stop(handle);
  uv_close(reinterpret_cast<uv_handle_t*>(handle), cameraCaptureClose);
  
  auto thisObj = v8::Local<v8::Object>::New(data->thisObj);
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  bool captured = camera_capture(camera);
  v8::Local<v8::Value> argv[] = {
    v8::Local<v8::Value>::New(v8::Boolean::New(captured))
  };
  data->callback->Call(thisObj, 1, argv);
  data->thisObj.Dispose();
  data->callback.Dispose();
  delete data;
}
static v8::Handle<v8::Value> cameraCapture(const v8::Arguments& args)
{
  v8::HandleScope scope;
  auto data = new CaptureData;
  data->thisObj = v8::Persistent<v8::Object>::New(args.This());
  data->callback = 
    v8::Persistent<v8::Function>::New(args[0].As<v8::Function>());
  auto xcamera = 
    v8::Local<v8::External>::Cast(data->thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());

  uv_poll_t* handle = new uv_poll_t;
  handle->data = data;
  uv_poll_init(uv_default_loop(), handle, camera->fd);
  uv_poll_start(handle, UV_READABLE, cameraCaptureCB);
  return v8::Undefined();
}

static v8::Handle<v8::Value> cameraToYUYV(const v8::Arguments& args)
{
  v8::HandleScope scope;
  auto thisObj = args.This();
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  int size = camera->width * camera->height * 2;
  auto ret  = v8::Array::New(size);
  for (int i = 0; i < size; i++) {
    ret->Set(i, v8::Integer::NewFromUnsigned(camera->head.start[i]));
  }
  return scope.Close(ret);
}

static v8::Handle<v8::Value> cameraToRGB(const v8::Arguments& args)
{
  v8::HandleScope scope;
  auto thisObj = args.This();
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  auto rgb = yuyv2rgb(camera->head.start, camera->width, camera->height);
  int size = camera->width * camera->height * 3;
  auto ret  = v8::Array::New(size);
  for (int i = 0; i < size; i++) {
    ret->Set(i, v8::Integer::NewFromUnsigned(rgb[i]));
  }
  free(rgb);
  return scope.Close(ret);
}

static v8::Handle<v8::Value> cameraControlGet(const v8::Arguments& args)
{
  v8::HandleScope scope;
  if (args.Length() < 1) {
    auto msg = v8::String::New("an argument required: id");
    return v8::ThrowException(v8::Exception::TypeError(msg));
  }
  uint32_t id = args[0]->Uint32Value();
  auto thisObj = args.This();
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  int32_t value = 0;
  bool success = camera_control_get(camera, id, &value);
  if (!success) return throwError(camera);
  return scope.Close(v8::Integer::New(value));
}

static v8::Handle<v8::Value> cameraControlSet(const v8::Arguments& args)
{
  v8::HandleScope scope;
  if (args.Length() < 2) {
    auto msg = v8::String::New("an argument required: id, value");
    return v8::ThrowException(v8::Exception::TypeError(msg));
  }
  uint32_t id = args[0]->Uint32Value();
  int32_t value = args[1]->Int32Value();
  auto thisObj = args.This();
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  bool success = camera_control_set(camera, id, value);
  if (!success) return throwError(camera);
  return scope.Close(thisObj);
}

static void moduleInit(v8::Handle<v8::Object> exports)
{
  v8::HandleScope scope;  
  auto clazz = v8::FunctionTemplate::New(cameraNew);
  clazz->SetClassName(v8::String::NewSymbol("Camera"));
  clazz->InstanceTemplate()->SetInternalFieldCount(1);
  auto proto = clazz->PrototypeTemplate();
  proto->Set(v8::String::NewSymbol("config"),
             v8::FunctionTemplate::New(cameraConfig)->GetFunction());
  proto->Set(v8::String::NewSymbol("start"),
             v8::FunctionTemplate::New(cameraStart)->GetFunction());
  proto->Set(v8::String::NewSymbol("stop"),
             v8::FunctionTemplate::New(cameraStop)->GetFunction());
  proto->Set(v8::String::NewSymbol("capture"),
             v8::FunctionTemplate::New(cameraCapture)->GetFunction());
  proto->Set(v8::String::NewSymbol("toYUYV"),
             v8::FunctionTemplate::New(cameraToYUYV)->GetFunction());
  proto->Set(v8::String::NewSymbol("toRGB"),
             v8::FunctionTemplate::New(cameraToRGB)->GetFunction());
  proto->Set(v8::String::NewSymbol("controlGet"),
             v8::FunctionTemplate::New(cameraControlGet)->GetFunction());
  proto->Set(v8::String::NewSymbol("controlSet"),
             v8::FunctionTemplate::New(cameraControlSet)->GetFunction());
  auto ctor = v8::Local<v8::Function>::New(clazz->GetFunction());
  exports->Set(v8::String::NewSymbol("Camera"), ctor);
}
NODE_MODULE(v4l2camera, moduleInit);

}
