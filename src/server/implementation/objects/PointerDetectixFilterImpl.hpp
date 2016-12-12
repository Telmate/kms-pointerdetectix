/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef __POINTER_DETECTOR_FILTER_IMPL_HPP__
#define __POINTER_DETECTOR_FILTER_IMPL_HPP__

#include "FilterImpl.hpp"
#include "PointerDetectixFilter.hpp"
#include <EventHandler.hpp>
#include <boost/property_tree/ptree.hpp>

namespace kurento
{
namespace module
{
namespace pointerdetectix
{
class PointerDetectixFilterImpl;
} /* pointerdetectix */
} /* module */
} /* kurento */

namespace kurento
{
void Serialize (
  std::shared_ptr<kurento::module::pointerdetectix::PointerDetectixFilterImpl>
  &object, JsonSerializer &serializer);
} /* kurento */

namespace kurento
{
namespace module
{
namespace pointerdetectix
{
class WindowParam;
class PointerDetectixWindowMediaParam;
} /* pointerdetectix */
} /* module */
} /* kurento */

namespace kurento
{
class MediaPipelineImpl;
} /* kurento */

namespace kurento
{
namespace module
{
namespace pointerdetectix
{

class PointerDetectixFilterImpl : public FilterImpl,
  public virtual PointerDetectixFilter
{

public:

  PointerDetectixFilterImpl (const boost::property_tree::ptree &config,
                             std::shared_ptr<MediaPipeline> mediaPipeline,
                             std::shared_ptr<WindowParam> calibrationRegion,
                             const std::vector<std::shared_ptr<PointerDetectixWindowMediaParam>> &windows);

  virtual ~PointerDetectixFilterImpl ();

  void addWindow (std::shared_ptr<PointerDetectixWindowMediaParam> window);
  void clearWindows ();
  void trackColorFromCalibrationRegion ();
  void removeWindow (const std::string &windowId);

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  sigc::signal<void, WindowIn> signalWindowIn;
  sigc::signal<void, WindowOut> signalWindowOut;
  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

protected:
  virtual void postConstructor ();

private:

  GstElement *pointerDetectix;
  gulong bus_handler_id;
  void busMessage (GstMessage *message);

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* pointerdetectix */
} /* module */
} /* kurento */

#endif /*  __POINTER_DETECTOR_FILTER_IMPL_HPP__ */
