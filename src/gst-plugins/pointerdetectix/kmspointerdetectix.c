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
#define _XOPEN_SOURCE 500

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <glib/gstdio.h>
#include <ftw.h>
#include <string.h>
#include <errno.h>
#include <libsoup/soup.h>

#include "kmspointerdetectix.h"
#include <commons/kms-core-marshal.h>

#define PLUGIN_NAME "pointerdetectix"

GST_DEBUG_CATEGORY_STATIC (kms_pointer_detectix_debug_category);
#define GST_CAT_DEFAULT kms_pointer_detectix_debug_category

#define KMS_POINTER_DETECTOR_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (                  \
    (obj),                                       \
    KMS_TYPE_POINTER_DETECTOR,                  \
    KmsPointerDetectixPrivate                   \
  )                                              \
)

#define FRAMES_TO_RESET  ((int) 250)
#define GREEN CV_RGB (0, 255, 0)
#define WHITE CV_RGB (255, 255, 255)
#define RED cvScalar (359, 89, 100, 0)

#define TEMP_PATH "/tmp/XXXXXX"
#define INACTIVE_IMAGE_VARIANT_NAME "i"
#define ACTIVE_IMAGE_VARIANT_NAME "a"
#define V_MIN 30
#define V_MAX 256
#define H_VALUES 181
#define S_VALUES 256
#define H_MAX 180
#define S_MAX 255
#define HIST_THRESHOLD 20

enum
{
  PROP_0,
  PROP_SHOW_DEBUG_INFO,
  PROP_WINDOWS_LAYOUT,
  PROP_MESSAGE,
  PROP_SHOW_WINDOWS_LAYOUT,
  PROP_CALIBRATION_AREA
};

enum
{
  SIGNAL_CALIBRATE_COLOR,
  LAST_SIGNAL
};

static guint kms_pointer_detectix_signals[LAST_SIGNAL] = { 0 };

struct _KmsPointerDetectixPrivate
{
  IplImage *cvImage;
  CvPoint finalPointerPosition;
  int iteration;
  CvSize frameSize;
  gboolean show_debug_info;
  GstStructure *buttonsLayout;
  GSList *buttonsLayoutList;
  gchar *previousButtonClickedId;
  gboolean putMessage;
  gboolean show_windows_layout;
  gchar *images_dir;
  gint x_calibration, y_calibration, width_calibration, height_calibration;
  gint h_min, h_max, s_min, s_max;
  IplConvKernel *kernel1;
  IplConvKernel *kernel2;
};

/* pad templates */

#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{ BGR }")

#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ BGR }")

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsPointerDetectix, kms_pointer_detectix,
    GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (kms_pointer_detectix_debug_category, PLUGIN_NAME,
        0, "debug category for pointerdetectix element"));


static guint DBG_Get_Millisec_Since_Startup()
{
    #define  NANOS_PER_MILLISEC  ((guint64) (1000L * 1000L))

    static GstClock      * The_Sys_Clock_Ptr = NULL;
    static GstClockTime    The_Startup_Nanos = 0;

    if (The_Sys_Clock_Ptr == NULL)
    {
        The_Sys_Clock_Ptr = gst_system_clock_obtain();

        The_Startup_Nanos = gst_clock_get_time (The_Sys_Clock_Ptr);
    }

    GstClockTime  now_nanos = gst_clock_get_time( The_Sys_Clock_Ptr );

    GstClockTime elapsed_ns = now_nanos -The_Startup_Nanos;

    guint  elapsed_millisec = (guint) (elapsed_ns / NANOS_PER_MILLISEC);

    return elapsed_millisec;
}


static void dispose_button_struct (gpointer data)
{
    ButtonStruct *aux = data;

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );


    if (aux->id != NULL)                g_free (aux->id);

    if (aux->inactive_icon != NULL)     cvReleaseImage (&aux->inactive_icon);

    if (aux->active_icon != NULL)       cvReleaseImage (&aux->active_icon);

    g_free (aux);
}


static void kms_pointer_detectix_dispose_buttons_layout_list (KmsPointerDetectix *  pointerdetectix)
{
    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    g_slist_free_full (pointerdetectix->priv->buttonsLayoutList, dispose_button_struct);
    pointerdetectix->priv->buttonsLayoutList = NULL;
}


static gboolean is_valid_uri (const gchar * url)
{
    gboolean ret;
    GRegex *regex;

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    regex = g_regex_new ("^(?:((?:https?):)\\/\\/)([^:\\/\\s]+)(?::(\\d*))?(?:\\/"
                         "([^\\s?#]+)?([?][^?#]*)?(#.*)?)?$", 0, 0, NULL);

    ret = g_regex_match (regex, url, G_REGEX_MATCH_ANCHORED, NULL);

    g_regex_unref (regex);

    return ret;
}


static void load_from_url (gchar * file_name, gchar * url)
{
    SoupSession *session;
    SoupMessage *msg;
    FILE *dst;

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    session = soup_session_sync_new ();
    msg = soup_message_new ("GET", url);

    soup_session_send_message (session, msg);

    dst = fopen (file_name, "w+");

    if (dst == NULL) 
    {
        GST_ERROR ("It is not possible to create the file");
        goto end;
    }

    fwrite (msg->response_body->data, 1, msg->response_body->length, dst);
    fclose (dst);

end:
    g_object_unref (msg);
    g_object_unref (session);
}


static IplImage * load_image (gchar * uri, gchar * dir, gchar * image_name, const gchar * name_variant)
{
    IplImage *aux = cvLoadImage (uri, CV_LOAD_IMAGE_UNCHANGED);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    if (aux == NULL) 
    {
        if (is_valid_uri (uri)) 
        {
            gchar *file_name = g_strconcat (dir, "/", image_name, name_variant, ".png", NULL);
            load_from_url (file_name, uri);
            aux = cvLoadImage (file_name, CV_LOAD_IMAGE_UNCHANGED);
            g_remove (file_name);
            g_free (file_name);
        }
    }

    return aux;
}


static void kms_pointer_detectix_load_buttonsLayout (KmsPointerDetectix * pointerdetectix)
{
    int aux, len;

    gboolean have_inactive_icon, have_active_icon, have_transparency;

    gchar *inactive_uri, *active_uri;

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__);

    if (pointerdetectix->priv->buttonsLayoutList != NULL)
    {
        kms_pointer_detectix_dispose_buttons_layout_list(pointerdetectix);
    }

    len = gst_structure_n_fields(pointerdetectix->priv->buttonsLayout);

    GST_DEBUG("len: %d", len);

    for (aux = 0; aux < len; aux++)
    {
        const gchar *name = gst_structure_nth_field_name(pointerdetectix->priv->buttonsLayout, aux);
        GstStructure *button;
        gboolean ret;

        ret = gst_structure_get(pointerdetectix->priv->buttonsLayout, name, GST_TYPE_STRUCTURE, &button, NULL);

        if (ret)
        {
            ButtonStruct *structAux = g_malloc0(sizeof(ButtonStruct));
            IplImage *aux = NULL;

            gst_structure_get(button, "upRightCornerX", G_TYPE_INT, &structAux->cvButtonLayout.x, NULL);
            gst_structure_get(button, "upRightCornerY", G_TYPE_INT, &structAux->cvButtonLayout.y, NULL);
            gst_structure_get(button, "width", G_TYPE_INT, &structAux->cvButtonLayout.width, NULL);
            gst_structure_get(button, "height", G_TYPE_INT, &structAux->cvButtonLayout.height, NULL);
            gst_structure_get(button, "id", G_TYPE_STRING, &structAux->id, NULL);
    
            have_inactive_icon = gst_structure_get(button, "inactive_uri", G_TYPE_STRING, &inactive_uri, NULL);

            have_transparency = gst_structure_get(button,
                                                  "transparency",
                                                  G_TYPE_DOUBLE,
                                                  &structAux->transparency,
                                                  NULL);

            have_active_icon = gst_structure_get(button, "active_uri", G_TYPE_STRING, &active_uri, NULL);

            if (have_inactive_icon)
            {
                aux = load_image(inactive_uri,
                                 pointerdetectix->priv->images_dir,
                                 structAux->id,
                                 INACTIVE_IMAGE_VARIANT_NAME);

                if (aux != NULL)
                {
                    structAux->inactive_icon = cvCreateImage(cvSize(structAux->cvButtonLayout.width,
                                                                    structAux->cvButtonLayout.height),
                                                             aux->depth,
                                                             aux->nChannels);
                    cvResize(aux, structAux->inactive_icon, CV_INTER_CUBIC);
                    cvReleaseImage(&aux);
                }
                else
                {
                    structAux->inactive_icon = NULL;
                }
            }
            else
            {
                structAux->inactive_icon = NULL;
            }

            if (have_active_icon)
            {
                aux = load_image(active_uri,
                                 pointerdetectix->priv->images_dir,
                                 structAux->id,
                                 ACTIVE_IMAGE_VARIANT_NAME);

                if (aux != NULL)
                {
                    structAux->active_icon = cvCreateImage(cvSize(structAux->cvButtonLayout.width,
                                                                  structAux->cvButtonLayout.height),
                                                           aux->depth,
                                                           aux->nChannels);
                    cvResize(aux, structAux->active_icon, CV_INTER_CUBIC);
                    cvReleaseImage(&aux);
                }
                else
                {
                    structAux->active_icon = NULL;
                }
            }
            else
            {
                structAux->active_icon = NULL;
            }

            if (have_transparency)
            {
                structAux->transparency = 1.0 - structAux->transparency;
            }
            else
            {
                structAux->transparency = 1.0;
            }

            GST_DEBUG("check: %d %d %d %d",
                      structAux->cvButtonLayout.x,
                      structAux->cvButtonLayout.y,
                      structAux->cvButtonLayout.width,
                      structAux->cvButtonLayout.height);

            pointerdetectix->priv->buttonsLayoutList = g_slist_append(pointerdetectix->priv->buttonsLayoutList, structAux);

            gst_structure_free(button);

            if (have_inactive_icon)
            {
                g_free(inactive_uri);
            }

            if (have_active_icon)
            {
                g_free(active_uri);
            }
        }
    }

    return;
}


static void kms_pointer_detectix_init (KmsPointerDetectix * pointerdetectix)
{
    pointerdetectix->priv = KMS_POINTER_DETECTOR_GET_PRIVATE (pointerdetectix);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    pointerdetectix->priv->cvImage = NULL;
    pointerdetectix->priv->iteration = 0;
    pointerdetectix->priv->show_debug_info = FALSE;
    pointerdetectix->priv->buttonsLayout = NULL;
    pointerdetectix->priv->buttonsLayoutList = NULL;
    pointerdetectix->priv->putMessage = TRUE;
    pointerdetectix->priv->show_windows_layout = TRUE;

    pointerdetectix->priv->h_min = 0;
    pointerdetectix->priv->h_max = 0;
    pointerdetectix->priv->s_min = 0;
    pointerdetectix->priv->s_max = 0;

    pointerdetectix->priv->x_calibration = 0;
    pointerdetectix->priv->y_calibration = 0;
    pointerdetectix->priv->width_calibration = 0;
    pointerdetectix->priv->height_calibration = 0;

    gchar d[] = TEMP_PATH;
    gchar *aux = g_mkdtemp (d);

    pointerdetectix->priv->images_dir = g_strdup (aux);
    pointerdetectix->priv->kernel1 = cvCreateStructuringElementEx (21, 21, 10, 10, CV_SHAPE_RECT, NULL);
    pointerdetectix->priv->kernel2 = cvCreateStructuringElementEx (11, 11, 5, 5, CV_SHAPE_RECT, NULL);
}


static void kms_pointer_detectix_calibrate_color (KmsPointerDetectix * pointerdetectix)
{
    gint h_values[H_VALUES];
    gint s_values[S_VALUES];
    IplImage *h_channel, *s_channel;
    IplImage *calibration_area;
    gint i, j;

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    if (pointerdetectix->priv->cvImage == NULL) 
    {
        return;
    }

    memset (h_values, 0, H_VALUES * sizeof (gint));
    memset (s_values, 0, S_VALUES * sizeof (gint));

    calibration_area = cvCreateImage (cvSize(pointerdetectix->priv->width_calibration, pointerdetectix->priv->height_calibration), 
                                      IPL_DEPTH_8U, 
                                      3);

    h_channel = cvCreateImage (cvGetSize (calibration_area), 8, 1);
    s_channel = cvCreateImage (cvGetSize (calibration_area), 8, 1);

    GST_DEBUG ("REGION x %d y %d  width %d height %d\n",
                pointerdetectix->priv->x_calibration,
                pointerdetectix->priv->y_calibration,
                pointerdetectix->priv->width_calibration,
                pointerdetectix->priv->height_calibration);

    GST_OBJECT_LOCK (pointerdetectix);

    cvSetImageROI (pointerdetectix->priv->cvImage, 
                   cvRect(pointerdetectix->priv->x_calibration,     
                          pointerdetectix->priv->y_calibration,
                          pointerdetectix->priv->width_calibration,
                          pointerdetectix->priv->height_calibration));

    cvCopy (pointerdetectix->priv->cvImage, calibration_area, 0);
    cvResetImageROI (pointerdetectix->priv->cvImage);

    cvCvtColor (calibration_area, calibration_area, CV_BGR2HSV);
    cvSplit (calibration_area, h_channel, s_channel, NULL, NULL);

    for (i = 0; i < calibration_area->width; i++) 
    {
        for (j = 0; j < calibration_area->height; j++) 
        {
            h_values[(*(uchar *) (h_channel->imageData + (j) * h_channel->widthStep + i))]++;
            s_values[(*(uchar *) (s_channel->imageData + (j) * s_channel->widthStep + i))]++;
        }
    }

    for (i = 1; i < H_MAX; i++) 
    {
        if (h_values[i] >= HIST_THRESHOLD) 
        {
            pointerdetectix->priv->h_min = i - 5;
            break;
        }
    }

    for (i = H_MAX; i >= 0; i--) 
    {
        if (h_values[i] >= HIST_THRESHOLD) 
        {
            pointerdetectix->priv->h_max = i + 5;
            break;
        }
    }

    for (i = 1; i < S_MAX; i++) 
    {
        if (s_values[i] >= HIST_THRESHOLD) 
        {
            pointerdetectix->priv->s_min = i - 5;
            break;
        }
    }

    for (i = S_MAX; i >= 0; i--) 
    {
        if (s_values[i] >= HIST_THRESHOLD) 
        {
            pointerdetectix->priv->s_max = i + 5;
            break;
        }
    }

    GST_OBJECT_UNLOCK (pointerdetectix);

    GST_DEBUG ("COLOR TO TRACK h_min %d h_max %d s_min %d s_max %d",
                pointerdetectix->priv->h_min, 
                pointerdetectix->priv->h_max, 
                pointerdetectix->priv->s_min, 
                pointerdetectix->priv->s_max);

    cvReleaseImage (&h_channel);
    cvReleaseImage (&s_channel);
    cvReleaseImage (&calibration_area);
}

void kms_pointer_detectix_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (object);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    GST_OBJECT_LOCK (pointerdetectix);

    switch (property_id) 
    {
        case PROP_SHOW_DEBUG_INFO:
            pointerdetectix->priv->show_debug_info = g_value_get_boolean (value);
            break;

        case PROP_WINDOWS_LAYOUT:
            if (pointerdetectix->priv->buttonsLayout != NULL)
            {
                gst_structure_free (pointerdetectix->priv->buttonsLayout);
            }
            pointerdetectix->priv->buttonsLayout = g_value_dup_boxed (value);
            kms_pointer_detectix_load_buttonsLayout (pointerdetectix);
            break;

        case PROP_MESSAGE:
            pointerdetectix->priv->putMessage = g_value_get_boolean (value);
            break;

        case PROP_SHOW_WINDOWS_LAYOUT:
            pointerdetectix->priv->show_windows_layout = g_value_get_boolean (value);
            break;

        case PROP_CALIBRATION_AREA:
        {
            GstStructure *aux;
            aux = g_value_dup_boxed (value);
            gst_structure_get (aux, "x", G_TYPE_INT, &pointerdetectix->priv->x_calibration, NULL);
            gst_structure_get (aux, "y", G_TYPE_INT, &pointerdetectix->priv->y_calibration, NULL);
            gst_structure_get (aux, "width", G_TYPE_INT, &pointerdetectix->priv->width_calibration, NULL);
            gst_structure_get (aux, "height", G_TYPE_INT, &pointerdetectix->priv->height_calibration, NULL);
            gst_structure_free (aux);
            break;
        }

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }

    GST_OBJECT_UNLOCK (pointerdetectix);
}


void kms_pointer_detectix_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (object);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    switch (property_id) 
    {
        case PROP_SHOW_DEBUG_INFO:
            g_value_set_boolean (value, pointerdetectix->priv->show_debug_info);
            break;

        case PROP_WINDOWS_LAYOUT:
            if (pointerdetectix->priv->buttonsLayout == NULL) 
            {
                pointerdetectix->priv->buttonsLayout = gst_structure_new_empty ("windows");
            }
            g_value_set_boxed (value, pointerdetectix->priv->buttonsLayout);
            break;

        case PROP_MESSAGE:
            g_value_set_boolean (value, pointerdetectix->priv->putMessage);
            break;

        case PROP_SHOW_WINDOWS_LAYOUT:
            g_value_set_boolean (value, pointerdetectix->priv->show_windows_layout);
            break;

        case PROP_CALIBRATION_AREA:
        {
            GstStructure *aux;

            aux = gst_structure_new ("calibration_area",
                                      "x", G_TYPE_INT, pointerdetectix->priv->x_calibration,
                                      "y", G_TYPE_INT, pointerdetectix->priv->y_calibration,
                                      "width", G_TYPE_INT, pointerdetectix->priv->width_calibration,
                                      "height", G_TYPE_INT, pointerdetectix->priv->height_calibration,
                                      NULL);

            g_value_set_boxed (value, aux);

            gst_structure_free (aux);
            break;
        }

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}


static int delete_file (const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = g_remove (fpath);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    if (rv) 
    {
        GST_WARNING ("Error deleting file: %s. %s", fpath, strerror (errno));
    }

    return rv;
}


static void remove_recursive (const gchar * path)
{
    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    nftw (path, delete_file, 64, FTW_DEPTH | FTW_PHYS);
}


void kms_pointer_detectix_finalize (GObject * object)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (object);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    cvReleaseImageHeader (&pointerdetectix->priv->cvImage);

    cvReleaseStructuringElement (&pointerdetectix->priv->kernel1);
    cvReleaseStructuringElement (&pointerdetectix->priv->kernel2);

    remove_recursive (pointerdetectix->priv->images_dir);
    g_free (pointerdetectix->priv->images_dir);

    if (pointerdetectix->priv->previousButtonClickedId != NULL) 
    {
        g_free (pointerdetectix->priv->previousButtonClickedId);
    }

    if (pointerdetectix->priv->buttonsLayoutList != NULL) 
    {
        kms_pointer_detectix_dispose_buttons_layout_list (pointerdetectix);
    }

    if (pointerdetectix->priv->buttonsLayout != NULL) 
    {
        gst_structure_free (pointerdetectix->priv->buttonsLayout);
    }

    G_OBJECT_CLASS (kms_pointer_detectix_parent_class)->finalize (object);
}


static gboolean kms_pointer_detectix_start (GstBaseTransform * trans)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (trans);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    GST_DEBUG_OBJECT (pointerdetectix, "start");

    return TRUE;
}


static gboolean kms_pointer_detectix_stop (GstBaseTransform * trans)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (trans);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    GST_DEBUG_OBJECT (pointerdetectix, "stop");

    return TRUE;
}


static gboolean kms_pointer_detectix_set_info (GstVideoFilter * filter, GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
    KmsPointerDetectix *pointerdetectix = KMS_POINTER_DETECTOR (filter);

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();
    g_printf("%-7d --- %s --- %s \n", elapses_ms, PLUGIN_NAME, __func__ );

    GST_DEBUG_OBJECT (pointerdetectix, "set_info");

    return TRUE;
}



static GstFlowReturn kms_pointer_detectix_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
    static guint idx = 0;

    guint elapses_ms = DBG_Get_Millisec_Since_Startup();

    if (frame != NULL)
    {
        g_printf("%-7d --- %s --- %s --- wdt=%d  hgt=%d  idx=%d \n", elapses_ms, PLUGIN_NAME, __func__, frame->info.width, frame->info.height, ++idx);
    }
    else if (filter != NULL)
    {
        g_printf("%-7d --- %s --- %s --- frame is NULL  \n", elapses_ms, PLUGIN_NAME, __func__);
    }
    else    // suppres warnings about unused statics
    {
        ; // kms_pointer_detectix_check_pointer_position(NULL);
    }

    return GST_FLOW_OK;
}


static void kms_pointer_detectix_class_init (KmsPointerDetectixClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

    GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

    g_printf("%s --- %s \n", PLUGIN_NAME, __func__ );

  /* Setting up pads and setting metadata should be moved to base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Pointer detectix element", "Video/Filter",
      "Detects pointer an raises events with its position",
      "Francisco Rivero <fj.riverog@gmail.com>");

  klass->calibrate_color =
      GST_DEBUG_FUNCPTR (kms_pointer_detectix_calibrate_color);

  gobject_class->set_property = kms_pointer_detectix_set_property;
  gobject_class->get_property = kms_pointer_detectix_get_property;
  gobject_class->finalize = kms_pointer_detectix_finalize;
  base_transform_class->start = GST_DEBUG_FUNCPTR (kms_pointer_detectix_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (kms_pointer_detectix_stop);
  video_filter_class->set_info =
      GST_DEBUG_FUNCPTR (kms_pointer_detectix_set_info);
  video_filter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (kms_pointer_detectix_transform_frame_ip);

  /* Properties initialization */
  g_object_class_install_property (gobject_class, PROP_SHOW_DEBUG_INFO,
      g_param_spec_boolean ("show-debug-region", "show debug region",
          "show evaluation regions over the image", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_WINDOWS_LAYOUT,
      g_param_spec_boxed ("windows-layout", "windows layout",
          "supply the positions and dimensions of windows into the main window",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MESSAGE,
      g_param_spec_boolean ("message", "message",
          "Put a window-in or window-out message in the bus if "
          "an object enters o leaves a window", TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SHOW_WINDOWS_LAYOUT,
      g_param_spec_boolean ("show-windows-layout", "show windows layout",
          "show windows layout over the image", TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CALIBRATION_AREA,
      g_param_spec_boxed ("calibration-area", "calibration area",
          "define the window used to calibrate the color to track",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  kms_pointer_detectix_signals[SIGNAL_CALIBRATE_COLOR] =
      g_signal_new ("calibrate-color",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsPointerDetectixClass, calibrate_color), NULL, NULL,
      __kms_core_marshal_VOID__VOID, G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (KmsPointerDetectixPrivate));
}


gboolean kms_pointer_detectix_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE, KMS_TYPE_POINTER_DETECTOR);
}

