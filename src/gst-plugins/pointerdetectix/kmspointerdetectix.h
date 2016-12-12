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
#ifndef _KMS_POINTER_DETECTOR_H_
#define _KMS_POINTER_DETECTOR_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <stdio.h>

G_BEGIN_DECLS
#define KMS_TYPE_POINTER_DETECTOR   (kms_pointer_detectix_get_type())
#define KMS_POINTER_DETECTOR(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_POINTER_DETECTOR,KmsPointerDetectix))
#define KMS_POINTER_DETECTOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_POINTER_DETECTOR,KmsPointerDetectixClass))
#define KMS_IS_POINTER_DETECTOR(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_POINTER_DETECTOR))
#define KMS_IS_POINTER_DETECTOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_POINTER_DETECTOR))
typedef struct _KmsPointerDetectix KmsPointerDetectix;
typedef struct _KmsPointerDetectixClass KmsPointerDetectixClass;
typedef struct _KmsPointerDetectixPrivate KmsPointerDetectixPrivate;

typedef struct _ButtonStruct {
    CvRect cvButtonLayout;
    gchar *id;
    IplImage* inactive_icon;
    IplImage* active_icon;
    gdouble transparency;
} ButtonStruct;

struct _KmsPointerDetectix {
  GstVideoFilter base_pointerdetectix;

  KmsPointerDetectixPrivate *priv;
};

struct _KmsPointerDetectixClass {
  GstVideoFilterClass base_pointerdetectix_class;

  /* Actions */
  void (*calibrate_color) (KmsPointerDetectix *pointerdetectix);
};

GType kms_pointer_detectix_get_type (void);

gboolean kms_pointer_detectix_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif
