#include "capture.h"
#include <node.h>
#include <v8.h>
#include <uv.h>
 

static void V4l2CameraDelete(v8::Persistent<v8::Value> handle, void* param)
{
  camera_t* camera = static_cast<camera_t*>(param);
  camera_finish(camera);
  camera_close(camera);
  handle.Dispose();
}

static v8::Handle<v8::Value> V4l2CameraNew(const v8::Arguments& args)
{
  v8::HandleScope scope;
  if (args.Length() < 3) {
    v8::Local<v8::String> msg = v8::String::New("Wrong number of arguments");
    v8::ThrowException(v8::Exception::TypeError(msg));
    return scope.Close(v8::Undefined());
  }
  v8::String::Utf8Value u8device(args[0]->ToString());
  char* device = *u8device;
  uint32_t width = args[1]->Uint32Value();
  uint32_t height = args[2]->Uint32Value();
  
  camera_t* camera = camera_open(device, width, height);
  camera_init(camera);
  v8::Local<v8::Object> thisObj = args.This();
  thisObj->SetInternalField(0, v8::External::New(camera));
  v8::Persistent<v8::Object> holder = v8::Persistent<v8::Object>::New(thisObj);
  holder.MakeWeak(camera, V4l2CameraDelete);
  thisObj->Set(v8::String::NewSymbol("device"), args[0]);
  thisObj->Set(v8::String::NewSymbol("width"), args[1]);
  thisObj->Set(v8::String::NewSymbol("height"), args[2]);
  return scope.Close(thisObj);
}

static v8::Handle<v8::Value> V4l2CameraStart(const v8::Arguments& args)
{
  v8::HandleScope scope;
  v8::Local<v8::Object> thisObj = args.This();
  v8::Local<v8::External> xcamera = 
    v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  camera_t* camera = static_cast<camera_t*>(xcamera->Value());
  camera_start(camera);
  return scope.Close(thisObj);
}

static v8::Handle<v8::Value> V4l2CameraStop(const v8::Arguments& args)
{
  v8::HandleScope scope;
  v8::Local<v8::Object> thisObj = args.This();
  v8::Local<v8::External> xcamera = 
    v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  camera_t* camera = static_cast<camera_t*>(xcamera->Value());
  camera_stop(camera);
  return scope.Close(thisObj);
}


struct CaptureData {
  v8::Persistent<v8::Object> thisObj;
  v8::Persistent<v8::Function> callback;
};

static void V4l2CameraCaptureCB(uv_poll_t* handle, int status, int events)
{
  uv_poll_stop(handle);
  v8::HandleScope scope;
  CaptureData* data = static_cast<CaptureData*>(handle->data);
  v8::Local<v8::Object> thisObj = v8::Local<v8::Object>::New(data->thisObj);
  v8::Local<v8::External> xcamera = 
    v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  camera_t* camera = static_cast<camera_t*>(xcamera->Value());
  
  camera_capture(camera);
  data->callback->Call(thisObj, 0, NULL);
  data->thisObj.Dispose();
  data->callback.Dispose();
  delete data;
  uv_unref(reinterpret_cast<uv_handle_t*>(handle));
  delete handle;
}


static v8::Handle<v8::Value> V4l2CameraCapture(const v8::Arguments& args)
{
  v8::HandleScope scope;
  CaptureData* data = new CaptureData;
  data->thisObj = v8::Persistent<v8::Object>::New(args.This());
  data->callback = 
    v8::Persistent<v8::Function>::New(args[0].As<v8::Function>());
  v8::Local<v8::External> xcamera = 
    v8::Local<v8::External>::Cast(data->thisObj->GetInternalField(0));
  camera_t* camera = static_cast<camera_t*>(xcamera->Value());

  uv_poll_t* handle = new uv_poll_t;
  handle->data = data;
  uv_poll_init(uv_default_loop(), handle, camera->fd);
  uv_poll_start(handle, UV_READABLE, V4l2CameraCaptureCB);
  uv_ref(reinterpret_cast<uv_handle_t*>(handle));
  return scope.Close(v8::Undefined());
}


static v8::Handle<v8::Value> V4l2CameraToRGB(const v8::Arguments& args)
{
  v8::HandleScope scope;
  v8::Local<v8::Object> thisObj = args.This();
  v8::Local<v8::External> xcamera = 
    v8::Local<v8::External>::Cast(thisObj->GetInternalField(0));
  camera_t* camera = static_cast<camera_t*>(xcamera->Value());
  uint8_t* rgb = yuyv2rgb(camera->head.start, camera->width, camera->height);
  int size = camera->width * camera->height * 3;
  v8::Handle<v8::Object> ret  = v8::Array::New(size);
  for (int i = 0; i < size; i++) {
    ret->Set(i, v8::Integer::NewFromUnsigned(rgb[i]));
  }
  free(rgb);
  return ret;
}

static void Init(v8::Handle<v8::Object> exports)
{
  v8::Local<v8::FunctionTemplate> clazz = 
    v8::FunctionTemplate::New(V4l2CameraNew);
  clazz->SetClassName(v8::String::NewSymbol("V4l2Camera"));
  clazz->InstanceTemplate()->SetInternalFieldCount(1);
  v8::Local<v8::ObjectTemplate> proto = clazz->PrototypeTemplate();
  proto->Set(v8::String::NewSymbol("start"),
             v8::FunctionTemplate::New(V4l2CameraStart)->GetFunction());
  proto->Set(v8::String::NewSymbol("stop"),
             v8::FunctionTemplate::New(V4l2CameraStop)->GetFunction());
  proto->Set(v8::String::NewSymbol("capture"),
             v8::FunctionTemplate::New(V4l2CameraCapture)->GetFunction());
  proto->Set(v8::String::NewSymbol("toRGB"),
             v8::FunctionTemplate::New(V4l2CameraToRGB)->GetFunction());
  
  v8::Persistent<v8::Function> ctor = 
    v8::Persistent<v8::Function>::New(clazz->GetFunction());
  exports->Set(v8::String::NewSymbol("V4l2Camera"), ctor);
}
NODE_MODULE(v4l2camera, Init);

