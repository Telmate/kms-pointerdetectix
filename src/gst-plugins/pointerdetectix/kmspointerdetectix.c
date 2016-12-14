/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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



#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include "kmspointerdetectix.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <glib/gstdio.h>


GST_DEBUG_CATEGORY_STATIC   (kms_pointer_detectix_debug_category);
#define GST_CAT_DEFAULT     kms_pointer_detectix_debug_category


typedef enum
{
    e_PROP_0,

    e_PROP_WAIT,    // "wait=MillisWaitBeforeNextFrameSnap"
    e_PROP_SNAP,    // "snap=MillisIntervals,MaxNumSnaps,MaxNumFails"
    e_PROP_LINK,    // "link=PipelineName,ProducerName,ConsumerName"
    e_PROP_PADS,    // "pads=ProducerOut,ConsumerInput,ConsumerOut"
    e_PROP_PATH,    // "path=PathForWorkingFolderForSavedImageFiles"
    e_PROP_NOTE,    // "note=none or note=MostRecentError"
    e_PROP_SILENT,   // silent=0 or silent=1 --- 1 disables messages

    e_PROP_SHOW_DEBUG_INFO,
    e_PROP_WINDOWS_LAYOUT,
    e_PROP_MESSAGE,
    e_PROP_SHOW_WINDOWS_LAYOUT,
    e_PROP_CALIBRATION_AREA

} PLUGIN_PARAMS_e;


enum
{
    e_FIRST_SIGNAL = 0,
    e_FINAL_SIGNAL

} PLUGIN_SIGNALS_e;


typedef struct _KmsPointerDetectixPrivate
{
    gboolean     is_silent, show_debug_info, putMessage, show_windows_layout;
    guint        num_buffs;
    guint        num_drops;
    guint        num_notes;
    gchar        sz_wait[30],
                 sz_snap[30],
                 sz_link[90],
                 sz_pads[90],
                 sz_path[400],
                 sz_note[200];

} KmsPointerDetectixPrivate;


#define VIDEO_SRC_CAPS  GST_VIDEO_CAPS_MAKE("{ BGR }")
#define VIDEO_SINK_CAPS GST_VIDEO_CAPS_MAKE("{ BGR }")


G_DEFINE_TYPE_WITH_CODE (KmsPointerDetectix,            \
                         kms_pointer_detectix,          \
                         GST_TYPE_VIDEO_FILTER,         \
                         GST_DEBUG_CATEGORY_INIT (kms_pointer_detectix_debug_category, \
                                                  THIS_PLUGIN_NAME, 0, \
                                                  "debug category for PointerDetectix element"));

#define GET_PRIVATE_STRUCT_PTR(obj) ( G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                                                   KMS_TYPE_POINTER_DETECTOR, \
                                                                   KmsPointerDetectixPrivate) )


#ifdef _SAVE_IMAGE_FRAMES_

    extern int Frame_Saver_Filter_Attach(GstElement * pluginPtr);
    extern int Frame_Saver_Filter_Detach(GstElement * pluginPtr);
    extern int Frame_Saver_Filter_Receive_Buffer(GstElement * pluginPtr, GstBuffer * aBufferPtr);
    extern int Frame_Saver_Filter_Transition(GstElement * pluginPtr, GstStateChange aTransition) ;
    extern int Frame_Saver_Filter_Set_Params(GstElement * pluginPtr, const gchar * aNewValuePtr, gchar * aPrvSpecsPtr);

#else

    static int Frame_Saver_Filter_Attach(GstElement * pluginPtr)
    {
        g_print("%s --- %s \n", THIS_PLUGIN_NAME, __func__);
        return 0;
    }
    static int Frame_Saver_Filter_Detach(GstElement * pluginPtr)
    {
        g_print("%s --- %s \n", THIS_PLUGIN_NAME, __func__);
        return 0;
    }
    static int Frame_Saver_Filter_Receive_Buffer(GstElement * pluginPtr, GstBuffer * aBufferPtr)
    {
        g_print("%s --- %s \n", THIS_PLUGIN_NAME, __func__);
        return 0;
    }
    static int Frame_Saver_Filter_Transition(GstElement * pluginPtr, GstStateChange aTransition)
    {
        g_print("%s --- %s \n", THIS_PLUGIN_NAME, __func__);
        return 0;
    }
    static int Frame_Saver_Filter_Set_Params(GstElement * pluginPtr, const gchar * aNewValuePtr, gchar * aPrvSpecsPtr)
    {
        g_print("%s --- %s \n", THIS_PLUGIN_NAME, __func__);
        return 0;
    }

#endif


static void initialize_plugin_instance(KmsPointerDetectix * aPluginPtr, KmsPointerDetectixPrivate * aPrivatePtr);


static GstClock      * The_Sys_Clock_Ptr = NULL;
static GstClockTime    The_Startup_Nanos = 0;


static guint64 Get_Runtime_Nanosec()
{
    if (The_Sys_Clock_Ptr == NULL)
    {
        The_Sys_Clock_Ptr = gst_system_clock_obtain();

        The_Startup_Nanos = gst_clock_get_time (The_Sys_Clock_Ptr);

        g_printf("\n%s\n", THIS_PLUGIN_NAME);
    }

    GstClockTime  now_nanos = gst_clock_get_time( The_Sys_Clock_Ptr );

    GstClockTime elapsed_ns = now_nanos -The_Startup_Nanos;

    return (guint64) elapsed_ns;
}


static guint Get_Runtime_Millisec()
{
    #define  NANOS_PER_MILLISEC  ((guint64) (1000L * 1000L))

    return (guint) (Get_Runtime_Nanosec() / NANOS_PER_MILLISEC);
}


static guint DBG_Print(const gchar * aTextPtr, gint aValue)
{
    guint elapsed_ms = Get_Runtime_Millisec();

    if (aTextPtr != NULL)
    {
        g_printf("%-7d --- %s --- %s --- (%d)", elapsed_ms, THIS_PLUGIN_NAME, aTextPtr, aValue);
    }

    g_printf("\n");

    return elapsed_ms;
}


static void kms_pointer_detectix_init (KmsPointerDetectix * pointerdetectix)
{
    The_Sys_Clock_Ptr = NULL;
    DBG_Print( __func__, 0 );

    initialize_plugin_instance(pointerdetectix, GET_PRIVATE_STRUCT_PTR (pointerdetectix));

    return;
}


void kms_pointer_detectix_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
    const gchar * psz_now = NULL;

    KmsPointerDetectix    * pointerdetectix = KMS_POINTER_DETECTOR (object);

    KmsPointerDetectixPrivate * ptr_private = GET_PRIVATE_STRUCT_PTR (pointerdetectix);

    DBG_Print( __func__, (gint) prop_id );

    GST_OBJECT_LOCK (pointerdetectix);

    switch (prop_id) 
    {
        case e_PROP_SILENT:
            ptr_private->is_silent = g_value_get_boolean(value);
            psz_now = ptr_private->is_silent ? "Silent=TRUE" : "Silent=FALSE";
            break;

        case e_PROP_WAIT:
            snprintf( ptr_private->sz_wait, sizeof(ptr_private->sz_wait), "wait=%s", g_value_get_string(value) );
            psz_now = ptr_private->sz_wait;
            break;

        case e_PROP_SNAP:
            snprintf( ptr_private->sz_snap, sizeof(ptr_private->sz_snap), "snap=%s", g_value_get_string(value) );
            psz_now = ptr_private->sz_snap;
            break;

        case e_PROP_LINK:
            snprintf( ptr_private->sz_link, sizeof(ptr_private->sz_link), "link=%s", g_value_get_string(value) );
            psz_now = ptr_private->sz_link;
            break;

        case e_PROP_PADS:
            snprintf( ptr_private->sz_pads, sizeof(ptr_private->sz_pads), "pads=%s", g_value_get_string(value) );
            psz_now = ptr_private->sz_pads;
            break;

        case e_PROP_PATH:
            snprintf( ptr_private->sz_path, sizeof(ptr_private->sz_path), "path=%s", g_value_get_string(value) );
            psz_now = ptr_private->sz_path;
            break;

        case e_PROP_SHOW_DEBUG_INFO:
            ptr_private->show_debug_info = g_value_get_boolean (value);
            break;

        case e_PROP_WINDOWS_LAYOUT:
            break;

        case e_PROP_MESSAGE:
            ptr_private->putMessage = g_value_get_boolean (value);
            break;

        case e_PROP_SHOW_WINDOWS_LAYOUT:
            ptr_private->show_windows_layout = g_value_get_boolean (value);
            break;

        case e_PROP_CALIBRATION_AREA:
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }

    if (psz_now != NULL)
    {
        g_print("%s --- Property #%u --- Now: (%s) \n", THIS_PLUGIN_NAME, (guint) prop_id, psz_now);

        ptr_private->num_buffs = 0;
        ptr_private->num_drops = 0;
        ptr_private->num_notes = 0;
    }

    GST_OBJECT_UNLOCK (pointerdetectix);

    return;
}


void kms_pointer_detectix_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (object);

    KmsPointerDetectixPrivate * ptr_private = GET_PRIVATE_STRUCT_PTR (pointerdetectix);

    DBG_Print( __func__, (gint) prop_id );

    GST_OBJECT_LOCK (pointerdetectix);

    switch (prop_id) 
    {
        case e_PROP_SILENT:
            g_value_set_boolean(value, ptr_private->is_silent);
            break;

        case e_PROP_WAIT:
            g_value_set_string(value, ptr_private->sz_wait);
            break;

        case e_PROP_SNAP:
            g_value_set_string(value, ptr_private->sz_snap);
            break;

        case e_PROP_LINK:
            g_value_set_string(value, ptr_private->sz_link);
            break;

        case e_PROP_PADS:
            g_value_set_string(value, ptr_private->sz_pads);
            break;

        case e_PROP_PATH:
            g_value_set_string(value, ptr_private->sz_path);
            break;

        case e_PROP_NOTE:
            g_value_set_string(value, ptr_private->sz_note);
            strcpy(ptr_private->sz_note, "note=none");
            break;

        case e_PROP_SHOW_DEBUG_INFO:
            g_value_set_boolean (value, ptr_private->show_debug_info);
            break;

        case e_PROP_WINDOWS_LAYOUT:
            break;

        case e_PROP_MESSAGE:
            g_value_set_boolean (value, ptr_private->putMessage);
            break;

        case e_PROP_SHOW_WINDOWS_LAYOUT:
            g_value_set_boolean (value, ptr_private->show_windows_layout);
            break;

        case e_PROP_CALIBRATION_AREA:
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }

    GST_OBJECT_UNLOCK (pointerdetectix);

    return;
}


void kms_pointer_detectix_finalize (GObject * object)
{
    DBG_Print( __func__, 0 );    DBG_Print( NULL, 0 );

    G_OBJECT_CLASS (kms_pointer_detectix_parent_class)->finalize (object);

    return;
}


static gboolean kms_pointer_detectix_start (GstBaseTransform * trans)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (trans);

    DBG_Print( __func__, 0 );

    GST_DEBUG_OBJECT (pointerdetectix, "start");

    return TRUE;
}


static gboolean kms_pointer_detectix_stop (GstBaseTransform * trans)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (trans);

    DBG_Print( __func__, 0 );

    GST_DEBUG_OBJECT (pointerdetectix, "stop");

    return TRUE;
}


static gboolean kms_pointer_detectix_set_info ( GstVideoFilter  * filter, 
                                                GstCaps         * in_caps_ptr, 
                                                GstVideoInfo    * in_info_ptr, 
                                                GstCaps         * out_caps_ptr, 
                                                GstVideoInfo    * out_info_ptr )
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (filter);

    DBG_Print( __func__, 0 );

    GST_DEBUG_OBJECT (pointerdetectix, "set_info");

    return TRUE;
}


static GstFlowReturn kms_pointer_detectix_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
    static gint  num_frames = 0;

    DBG_Print( __func__, (frame == NULL) ? 0 : ++num_frames );

    return GST_FLOW_OK;
}


static void kms_pointer_detectix_class_init (KmsPointerDetectixClass * klass)
{
    #define PARAM_ATTRIBUTES (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY)

    #ifdef _ALLOW_DYNAMIC_PARAMS_
        GParamFlags param_flags = (GParamFlags) (PARAM_ATTRIBUTES | GST_PARAM_CONTROLLABLE);
    #else
        GParamFlags param_flags = (GParamFlags) (PARAM_ATTRIBUTES);
    #endif

    GObjectClass            *        gobject_class_ptr = G_OBJECT_CLASS(klass);
    GstVideoFilterClass     *   video_filter_class_ptr = GST_VIDEO_FILTER_CLASS(klass);
    GstBaseTransformClass   * base_transform_class_ptr = GST_BASE_TRANSFORM_CLASS(klass);

    The_Sys_Clock_Ptr = NULL;    DBG_Print( __func__, 0 );

    gobject_class_ptr->set_property = kms_pointer_detectix_set_property;
    gobject_class_ptr->get_property = kms_pointer_detectix_get_property;
    gobject_class_ptr->finalize     = kms_pointer_detectix_finalize;

    base_transform_class_ptr->start = GST_DEBUG_FUNCPTR (kms_pointer_detectix_start);
    base_transform_class_ptr->stop  = GST_DEBUG_FUNCPTR (kms_pointer_detectix_stop);

    video_filter_class_ptr->set_info = GST_DEBUG_FUNCPTR (kms_pointer_detectix_set_info);
    video_filter_class_ptr->transform_frame_ip = GST_DEBUG_FUNCPTR (kms_pointer_detectix_transform_frame_ip);

    gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
                                        gst_pad_template_new ("src", 
                                                              GST_PAD_SRC, 
                                                              GST_PAD_ALWAYS,
                                                              gst_caps_from_string (VIDEO_SRC_CAPS)));

    gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
                                        gst_pad_template_new ("sink", 
                                                              GST_PAD_SINK, 
                                                              GST_PAD_ALWAYS,
                                                              gst_caps_from_string (VIDEO_SINK_CAPS)));
      
    g_object_class_install_property(gobject_class_ptr,
                                    e_PROP_WAIT,
                                    g_param_spec_string("wait",
                                                        "wait=MillisWaitBeforeNextFrameSnap",
                                                        "wait before snapping another frame",
                                                        "3000",
                                                        param_flags));

    g_object_class_install_property(gobject_class_ptr,
                                    e_PROP_SNAP,
                                    g_param_spec_string("snap",
                                                        "snap=millisecInterval,maxNumSnaps,maxNumFails",
                                                        "snap and save frames as RGB images in PNG files",
                                                        "1000,0,0",
                                                        param_flags));

    g_object_class_install_property(gobject_class_ptr,
                                    e_PROP_LINK,
                                    g_param_spec_string("link",
                                                        "link=pipelineName,producerName,consumerName",
                                                        "insert TEE between producer and consumer elements",
                                                        "auto,auto,auto",
                                                        param_flags));

    g_object_class_install_property(gobject_class_ptr,
                                    e_PROP_PADS,
                                    g_param_spec_string("pads",
                                                        "pads=producerOut,consumerInput,consumerOut",
                                                        "pads used by the producer and consumer elements",
                                                        "src,sink,src",
                                                        param_flags));

    g_object_class_install_property(gobject_class_ptr,
                                    e_PROP_PATH,
                                    g_param_spec_string("path",
                                                        "path=path-of-working-folder-for-saved-images",
                                                        "path of working folder for saved image files",
                                                        "auto",
                                                        param_flags));

    g_object_class_install_property(gobject_class_ptr,
                                    e_PROP_NOTE,
                                    g_param_spec_string("note",
                                                        "note=MostRecentErrorCodition",
                                                        "most recent error",
                                                        "none",
                                                        G_PARAM_READABLE));

    g_object_class_install_property(gobject_class_ptr,
                                    e_PROP_SILENT,
                                    g_param_spec_boolean("silent",
                                                         "Silent or Verbose",
                                                         "Silent is 1/True --- Verbose is 0/False",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class_ptr, 
                                     e_PROP_SHOW_DEBUG_INFO,
                                     g_param_spec_boolean ("show-debug-region", 
                                                           "show debug region",
                                                           "show evaluation regions over the image", 
                                                           FALSE, 
                                                           G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class_ptr, 
                                     e_PROP_WINDOWS_LAYOUT,
                                     g_param_spec_boxed ("windows-layout", 
                                                         "windows layout",
                                                         "supply the positions and dimensions of windows into the main window",
                                                         GST_TYPE_STRUCTURE, 
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class_ptr, 
                                     e_PROP_MESSAGE,
                                     g_param_spec_boolean ("message", 
                                                           "message",
                                                           "Put a window-in or window-out message in the bus if " "an object enters o leaves a window", 
                                                           TRUE, 
                                                           G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class_ptr, 
                                     e_PROP_SHOW_WINDOWS_LAYOUT,
                                     g_param_spec_boolean ("show-windows-layout", 
                                                           "show windows layout",
                                                           "show windows layout over the image", 
                                                           TRUE, 
                                                           G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class_ptr, 
                                     e_PROP_CALIBRATION_AREA,
                                     g_param_spec_boxed ("calibration-area", 
                                                         "calibration area",
                                                         "define the window used to calibrate the color to track",
                                                         GST_TYPE_STRUCTURE, 
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_set_details_simple(GST_ELEMENT_CLASS(klass),
                                         THIS_PLUGIN_NAME,                  // name to launch
                                         "Pointer-Detection-Video-Filter",  // classification
                                         "Detect Pointers In Video Frames", // description
                                         "Francisco Rivero <fj.riverog@gmail.com>");

    g_type_class_add_private (klass, sizeof (KmsPointerDetectixPrivate));
}


/*
 * entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean register_this_plugin(GstPlugin * aPluginPtr)
{
    The_Sys_Clock_Ptr = NULL;
    DBG_Print( __func__, 0 );

    return gst_element_register (aPluginPtr, THIS_PLUGIN_NAME, GST_RANK_NONE, KMS_TYPE_POINTER_DETECTOR);
}


// gstreamer looks for this structure to register plugins
GST_PLUGIN_DEFINE ( GST_VERSION_MAJOR, 
                    GST_VERSION_MINOR,
                    PointerDetectixVideoFilter, 
                    "Kurento pointer detectix", 
                    register_this_plugin, 
                    VERSION, GST_LICENSE_UNKNOWN, "Kurento", "http://kurento.com/" )


static void initialize_plugin_instance(KmsPointerDetectix * aPluginPtr, KmsPointerDetectixPrivate * aPrivatePtr)
{
    strcpy(aPrivatePtr->sz_wait, "wait=2000");
    strcpy(aPrivatePtr->sz_snap, "snap=0,0,0");
    strcpy(aPrivatePtr->sz_link, "link=Live,auto,auto");
    strcpy(aPrivatePtr->sz_pads, "pads=auto,auto,auto");
    strcpy(aPrivatePtr->sz_path, "path=auto");
    strcpy(aPrivatePtr->sz_note, "note=none");
    
    aPrivatePtr->num_buffs = 0;
    aPrivatePtr->num_drops = 0;    
    aPrivatePtr->num_notes = 0;
    aPrivatePtr->is_silent = TRUE;

    aPrivatePtr->show_debug_info    = FALSE;
    aPrivatePtr->putMessage         = TRUE;
    aPrivatePtr->show_windows_layout= TRUE;

    aPluginPtr->priv = aPrivatePtr;

    if (aPluginPtr == NULL)    // always FALSE, suppress warnings on unused statics
    {
        Frame_Saver_Filter_Attach(NULL);
        Frame_Saver_Filter_Detach(NULL);
        Frame_Saver_Filter_Transition(NULL, 0) ;
        Frame_Saver_Filter_Receive_Buffer(NULL, NULL);
        Frame_Saver_Filter_Set_Params(NULL, NULL, NULL);
    }  

    return;
}

// ends file:  "kmspointerdetectix.c"

