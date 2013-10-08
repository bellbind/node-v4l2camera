#include "capture.h"
#include <node.h>
#include <v8.h>
#include <uv.h>
#include <errno.h>
#include <string.h>

#include <string>
#include <sstream>

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

static void cameraDelete(v8::Persistent<v8::Value> handle, void* param)
{
  auto camera = static_cast<camera_t*>(param);
  camera_finish(camera);
  delete static_cast<LogContext*>(camera->context.pointer);
  camera_close(camera);
  handle.Dispose();
}

static v8::Handle<v8::Value> cameraNew(const v8::Arguments& args)
{
  v8::HandleScope scope;
  if (args.Length() < 3) {
    auto msg = v8::String::New("Wrong number of arguments");
    v8::ThrowException(v8::Exception::TypeError(msg));
    return scope.Close(v8::Undefined());
  }
  v8::String::Utf8Value u8device(args[0]->ToString());
  const char* device = *u8device;
  uint32_t width = args[1]->Uint32Value();
  uint32_t height = args[2]->Uint32Value();
  
  auto camera = camera_open(device, width, height);
  if (!camera) return throwError(strerror(errno));
  camera->context.pointer = new LogContext;
  camera->context.log = &logRecord;
  if (!camera_init(camera)) {
    auto err = throwError(camera);
    delete static_cast<LogContext*>(camera->context.pointer);
    return err;
  }
  auto thisObj = args.This();
  thisObj->SetInternalField(0, v8::External::New(camera));
  auto holder = v8::Persistent<v8::Object>::New(thisObj);
  holder.MakeWeak(camera, cameraDelete);
  thisObj->Set(v8::String::NewSymbol("device"), args[0]);
  thisObj->Set(v8::String::NewSymbol("width"), args[1]);
  thisObj->Set(v8::String::NewSymbol("height"), args[2]);
  return scope.Close(thisObj);
}

static v8::Handle<v8::Value> cameraStart(const v8::Arguments& args)
{
  v8::HandleScope scope;
  auto thisObj = args.This();
  auto xcamera = v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  auto camera = static_cast<camera_t*>(xcamera->Value());
  if (!camera_start(camera)) return throwError(camera);
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

static void moduleInit(v8::Handle<v8::Object> exports)
{
  v8::HandleScope scope;  
  auto clazz = v8::FunctionTemplate::New(cameraNew);
  clazz->SetClassName(v8::String::NewSymbol("Camera"));
  clazz->InstanceTemplate()->SetInternalFieldCount(1);
  auto proto = clazz->PrototypeTemplate();
  proto->Set(v8::String::NewSymbol("start"),
             v8::FunctionTemplate::New(cameraStart)->GetFunction());
  proto->Set(v8::String::NewSymbol("stop"),
             v8::FunctionTemplate::New(cameraStop)->GetFunction());
  proto->Set(v8::String::NewSymbol("capture"),
             v8::FunctionTemplate::New(cameraCapture)->GetFunction());
  proto->Set(v8::String::NewSymbol("toRGB"),
             v8::FunctionTemplate::New(cameraToRGB)->GetFunction());
  proto->Set(v8::String::NewSymbol("toYUYV"),
             v8::FunctionTemplate::New(cameraToYUYV)->GetFunction());
  auto ctor = v8::Persistent<v8::Function>::New(clazz->GetFunction());
  exports->Set(v8::String::NewSymbol("Camera"), ctor);
}
NODE_MODULE(v4l2camera, moduleInit);

}
