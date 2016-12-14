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

#include <gst/gst.h>
#include "MediaPipeline.hpp"
#include "MediaPipelineImpl.hpp"
#include "WindowParam.hpp"
#include "PointerDetectixWindowMediaParam.hpp"
#include <PointerDetectixFilterImplFactory.hpp>
#include "PointerDetectixFilterImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include "SignalHandler.hpp"

#define GST_CAT_DEFAULT kurento_pointer_detectix_filter_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoPointerDetectixFilterImpl"

#define WINDOWS_LAYOUT "windows-layout"
#define CALIBRATION_AREA "calibration-area"
#define CALIBRATE_COLOR "calibrate-color"

namespace kurento
{
namespace module
{
namespace pointerdetectix
{
static GstStructure *
get_structure_from_window (std::shared_ptr<PointerDetectixWindowMediaParam>
                           window)
{
  GstStructure *buttonsLayoutAux;

  buttonsLayoutAux = gst_structure_new (
                       window->getId().c_str(),
                       "upRightCornerX", G_TYPE_INT, window->getUpperRightX(),
                       "upRightCornerY", G_TYPE_INT, window->getUpperRightY(),
                       "width", G_TYPE_INT, window->getWidth(),
                       "height", G_TYPE_INT, window->getHeight(),
                       "id", G_TYPE_STRING, window->getId().c_str(),
                       NULL);

  if (window->isSetImage() ) {
    gst_structure_set (buttonsLayoutAux, "inactive_uri",
                       G_TYPE_STRING, window->getImage().c_str(), NULL);
  }

  if (window->isSetImageTransparency() ) {
    gst_structure_set (buttonsLayoutAux, "transparency",
                       G_TYPE_DOUBLE, double (window->getImageTransparency() ), NULL);
  }

  if (window->isSetActiveImage() ) {
    gst_structure_set (buttonsLayoutAux, "active_uri",
                       G_TYPE_STRING, window->getActiveImage().c_str(), NULL);
  }

  return buttonsLayoutAux;
}

void PointerDetectixFilterImpl::busMessage (GstMessage *message)
{
  const GstStructure *st;
  gchar *windowID;
  const gchar *type;
  std::string windowIDStr, typeStr;

  if (GST_MESSAGE_SRC (message) != GST_OBJECT (mNativeElementPtr) ||
      GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT) {
    return;
  }

  st = gst_message_get_structure (message);
  type = gst_structure_get_name (st);

  if ( (g_strcmp0 (type, "window-out") != 0) &&
       (g_strcmp0 (type, "window-in") != 0) ) {
    GST_WARNING ("The message does not have the correct name");
    return;
  }

  if (!gst_structure_get (st, "window", G_TYPE_STRING , &windowID, NULL) ) {
    GST_WARNING ("The message does not contain the window ID");
    return;
  }

  windowIDStr = windowID;
  typeStr = type;

  g_free (windowID);

  if (typeStr == "window-in") {
    try {
      WindowIn event (shared_from_this(), WindowIn::getName(), windowIDStr );

      signalWindowIn (event);
    } catch (std::bad_weak_ptr &e) {
    }
  } else if (typeStr == "window-out") {
    try {
      WindowOut event (shared_from_this(), WindowOut::getName(), windowIDStr );

      signalWindowOut (event);
    } catch (std::bad_weak_ptr &e) {
    }
  }
}

void PointerDetectixFilterImpl::postConstructor ()
{
  GstBus *bus;
  std::shared_ptr<MediaPipelineImpl> pipe;

  FilterImpl::postConstructor ();

  pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe->getPipeline() ) );

  bus_handler_id = register_signal_handler (G_OBJECT (bus),
                   "message",
                   std::function <void (GstElement *, GstMessage *) >
                   (std::bind (&PointerDetectixFilterImpl::busMessage, this,
                               std::placeholders::_2) ),
                   std::dynamic_pointer_cast<PointerDetectixFilterImpl>
                   (shared_from_this() ) );

  g_object_unref (bus);
}

PointerDetectixFilterImpl::PointerDetectixFilterImpl (const
    boost::property_tree::ptree &config,
    std::shared_ptr<MediaPipeline> mediaPipeline,
    std::shared_ptr<WindowParam> calibrationRegion,
    const std::vector<std::shared_ptr<PointerDetectixWindowMediaParam>> &windows)  :
  FilterImpl (config, std::dynamic_pointer_cast<MediaPipelineImpl>
              (mediaPipeline) )
{
  GstStructure *calibrationArea;
  GstStructure *buttonsLayout;

  g_object_set (element, "filter-factory", "pointerdetectix", NULL);

  g_object_get (G_OBJECT (element), "filter", &mNativeElementPtr, NULL);

  if (mNativeElementPtr == NULL) {
    g_object_unref (bus);
    throw KurentoException (MEDIA_OBJECT_NOT_AVAILABLE,
                            "Media Object not available");
  }

  calibrationArea = gst_structure_new (
                      "calibration_area",
                      "x", G_TYPE_INT, calibrationRegion->getTopRightCornerX(),
                      "y", G_TYPE_INT, calibrationRegion->getTopRightCornerY(),
                      "width", G_TYPE_INT, calibrationRegion->getWidth(),
                      "height", G_TYPE_INT, calibrationRegion->getHeight(),
                      NULL);
  g_object_set (G_OBJECT (mNativeElementPtr), CALIBRATION_AREA,
                calibrationArea, NULL);
  gst_structure_free (calibrationArea);

  buttonsLayout = gst_structure_new_empty  ("windowsLayout");

  for (auto window : windows) {
    GstStructure *buttonsLayoutAux = get_structure_from_window (window);

    gst_structure_set (buttonsLayout,
                       window->getId().c_str(), GST_TYPE_STRUCTURE,
                       buttonsLayoutAux,
                       NULL);

    gst_structure_free (buttonsLayoutAux);
  }

  g_object_set (G_OBJECT (mNativeElementPtr), WINDOWS_LAYOUT, buttonsLayout,
                NULL);
  gst_structure_free (buttonsLayout);

  bus_handler_id = 0;
  // There is no need to reference pointerdetectix because its life cycle is the same as the filter life cycle
  g_object_unref (mNativeElementPtr);
}

PointerDetectixFilterImpl::~PointerDetectixFilterImpl()
{
  std::shared_ptr<MediaPipelineImpl> pipe;

  if (bus_handler_id > 0) {
    pipe = std::dynamic_pointer_cast<MediaPipelineImpl> (getMediaPipeline() );
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipe->getPipeline() ) );
    unregister_signal_handler (bus, bus_handler_id);
    g_object_unref (bus);
  }
}

bool PointerDetectixFilterImpl::startPipelinePlaying()
{
    std::unique_lock <std::recursive_mutex>  locker (mRecursiveMutex);

    bool is_ok = (mNativeElementPtr != NULL);

    if (is_ok)
    {
        GstElement * pipeline_ptr = (GstElement *) gst_element_get_parent( mNativeElementPtr );

        if (pipeline_ptr != NULL)
        {
            GstState pipeline_state = GST_STATE_NULL;

            gst_element_get_state(pipeline_ptr, &pipeline_state, NULL, 0);

            if (pipeline_state != GST_STATE_PLAYING)
            {
                gst_element_set_state ( pipeline_ptr, GST_STATE_PLAYING );
            }
        }
        else
        {
            is_ok = false;
        }
    }

    mLastErrorDetails.assign( is_ok ? "" : "ERROR" );

    return is_ok;
}


bool PointerDetectixFilterImpl::stopPipelinePlaying()
{
    std::unique_lock <std::recursive_mutex>  locker (mRecursiveMutex);

    bool is_ok = (mNativeElementPtr != NULL);

    if (is_ok)
    {
        GstElement * pipeline_ptr = (GstElement *) gst_element_get_parent( mNativeElementPtr );

        if (pipeline_ptr != NULL)
        {
            GstState pipeline_state = GST_STATE_NULL;

            gst_element_get_state(pipeline_ptr, &pipeline_state, NULL, 0);

            if (pipeline_state == GST_STATE_PLAYING)
            {
                gst_element_set_state ( pipeline_ptr, GST_STATE_READY );
            }
        }
        else
        {
            is_ok = false;
        }
    }

    mLastErrorDetails.assign( is_ok ? "" : "ERROR" );

    return is_ok;
}


std::string PointerDetectixFilterImpl::getLastError()
{
    std::unique_lock <std::recursive_mutex>  locker(mRecursiveMutex);

    std::string  last_error( mLastErrorDetails.c_str() );

    mLastErrorDetails.assign("");

    return last_error;
}


std::string PointerDetectixFilterImpl::getElementsNamesList()
{
    std::unique_lock <std::recursive_mutex>  locker(mRecursiveMutex);

    std::string  names_separated_by_tabs;

    bool is_ok = (mNativeElementPtr != NULL);

    GstElement * pipeline_ptr = is_ok ? (GstElement *) gst_element_get_parent(mNativeElementPtr) : NULL;

    GstIterator * iterate_ptr = pipeline_ptr ? gst_bin_iterate_elements(GST_BIN(pipeline_ptr)) : NULL;

    is_ok = (iterate_ptr != NULL);

    if (is_ok)
    {
        gchar * name_ptr = gst_element_get_name(pipeline_ptr);

        names_separated_by_tabs.append(name_ptr ? name_ptr : "PipelineNameIsNull");
        names_separated_by_tabs.append("\t");

        g_free(name_ptr);

        GValue element_as_value = G_VALUE_INIT;

        while (gst_iterator_next(iterate_ptr, &element_as_value) == GST_ITERATOR_OK)
        {
            name_ptr = gst_element_get_name( g_value_get_object(&element_as_value) );

            names_separated_by_tabs.append(name_ptr ? name_ptr : "ElementNameIsNull");
            names_separated_by_tabs.append("\t");

            g_free(name_ptr);

            g_value_reset( &element_as_value );
        }

        g_value_unset( &element_as_value );

        gst_iterator_free(iterate_ptr);
    }

    mLastErrorDetails.assign( is_ok ? "" : "ERROR" );

    return names_separated_by_tabs;
}


std::string PointerDetectixFilterImpl::getParamsList()
{
    static const char * names[] = { "wait", "snap", "link", "pads", "path", "note", NULL };

    std::string  params_separated_by_tabs;

    int index = -1;

    while ( names[++index] != NULL )
    {
        std::string param_text = getParam( std::string(names[index]) );

        bool is_ok = mLastErrorDetails.empty();

        if (! is_ok)
        {
            params_separated_by_tabs.assign("");
            break;
        }

        params_separated_by_tabs.append( names[index] ); 
        params_separated_by_tabs.append( "=" );

        params_separated_by_tabs.append( param_text.c_str() );
        params_separated_by_tabs.append( "\t" );
    }

    return params_separated_by_tabs;
}


std::string PointerDetectixFilterImpl::getParam(const std::string & rParamName)
{
    std::unique_lock <std::recursive_mutex>  locker (mRecursiveMutex);

    gchar * text_ptr = NULL;

    bool is_ok = (mNativeElementPtr != NULL);

    if (is_ok)
    {
        g_object_get( G_OBJECT(mNativeElementPtr), rParamName.c_str(), & text_ptr, NULL );

        is_ok = (text_ptr != NULL);
    }

    std::string param_value(is_ok ? text_ptr : "");

    mLastErrorDetails.assign(is_ok ? "" : "ERROR");

    return param_value;
}


bool PointerDetectixFilterImpl::setParam(const std::string & rParamName, const std::string & rNewValue)
{
    std::unique_lock <std::recursive_mutex>  locker (mRecursiveMutex);

    std::string param_text = getParam(rParamName);

    bool is_ok = mLastErrorDetails.empty();

    if (is_ok)
    {
        g_object_set( G_OBJECT(mNativeElementPtr), rParamName.c_str(), rNewValue.c_str(), NULL );
    }

    mLastErrorDetails.assign( is_ok ? "" : "ERROR" );

    return is_ok;
}

void PointerDetectixFilterImpl::addWindow (
  std::shared_ptr<PointerDetectixWindowMediaParam> window)
{
  GstStructure *buttonsLayout, *buttonsLayoutAux;

  buttonsLayoutAux = get_structure_from_window (window);

  /* The function obtains the actual window list */
  g_object_get (G_OBJECT (mNativeElementPtr), WINDOWS_LAYOUT, &buttonsLayout,
                NULL);
  gst_structure_set (buttonsLayout,
                     window->getId().c_str(), GST_TYPE_STRUCTURE,
                     buttonsLayoutAux, NULL);

  g_object_set (G_OBJECT (mNativeElementPtr), WINDOWS_LAYOUT, buttonsLayout, NULL);

  gst_structure_free (buttonsLayout);
  gst_structure_free (buttonsLayoutAux);
}

void PointerDetectixFilterImpl::clearWindows ()
{
  GstStructure *buttonsLayout;

  buttonsLayout = gst_structure_new_empty  ("buttonsLayout");
  g_object_set (G_OBJECT (mNativeElementPtr), WINDOWS_LAYOUT, buttonsLayout,
                NULL);
  gst_structure_free (buttonsLayout);
}

void PointerDetectixFilterImpl::trackColorFromCalibrationRegion ()
{
  g_signal_emit_by_name (mNativeElementPtr, CALIBRATE_COLOR, NULL);
}

void PointerDetectixFilterImpl::removeWindow (const std::string &windowId)
{
  GstStructure *buttonsLayout;
  gint len;

  /* The function obtains the actual window list */
  g_object_get (G_OBJECT (mNativeElementPtr), WINDOWS_LAYOUT, &buttonsLayout,
                NULL);
  len = gst_structure_n_fields (buttonsLayout);

  if (len == 0) {
    GST_WARNING ("There are no windows in the layout");
    return;
  }

  for (int i = 0; i < len; i++) {
    const gchar *name;
    name = gst_structure_nth_field_name (buttonsLayout, i);

    if (windowId == name) {
      /* this window will be removed */
      gst_structure_remove_field (buttonsLayout, name);
    }
  }

  /* Set the buttons layout list without the window with id = id */
  g_object_set (G_OBJECT (mNativeElementPtr), WINDOWS_LAYOUT, buttonsLayout, NULL);

  gst_structure_free (buttonsLayout);
}

MediaObjectImpl *
PointerDetectixFilterImplFactory::createObject (const
    boost::property_tree::ptree &config,
    std::shared_ptr<MediaPipeline> mediaPipeline,
    std::shared_ptr<WindowParam> calibrationRegion,
    const std::vector<std::shared_ptr<PointerDetectixWindowMediaParam>> &windows)
const
{
  return new PointerDetectixFilterImpl (config, mediaPipeline, calibrationRegion,
                                        windows);
}

PointerDetectixFilterImpl::StaticConstructor
PointerDetectixFilterImpl::staticConstructor;

PointerDetectixFilterImpl::StaticConstructor::StaticConstructor()
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* pointerdetectix */
} /* module */
} /* kurento */
