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

#include <boost/property_tree/ptree.hpp>
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <EventHandler.hpp>
#include <mutex>


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

    bool startPipelinePlaying();                                    // starts pipeline PLAYING

    bool stopPipelinePlaying();                                     // changes PLAYING to READY

    std::string getLastError();                                     // returns non-empty for errors

    std::string getElementsNamesList();                             // returns NamesSeparatedByTabs

    std::string getParamsList();                                    // returns ParamsSeparatedByTabs

    std::string getParam(const std::string & rParamName);           // returns empty if invalid name

    bool setParam(const std::string & rParamName, const std::string & rNewValue); // FALSE if failed

    sigc::signal<void, WindowIn> signalWindowIn;
    sigc::signal<void, WindowOut> signalWindowOut;

    /* Next methods are automatically implemented by code generator */
    virtual void Serialize (JsonSerializer &serializer);
    virtual bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);
    virtual void invoke (std::shared_ptr<MediaObjectImpl> obj, const std::string &methodName, const Json::Value &params, Json::Value &response);

protected:
  virtual void postConstructor ();

private:
    std::recursive_mutex    mRecursiveMutex;
    std::string             mLastErrorDetails;
    GstElement            * mNativeElementPtr;
    gulong                  bus_handler_id;

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

