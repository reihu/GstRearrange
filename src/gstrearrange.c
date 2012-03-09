/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2012 Manuel Reithuber <gstreamer@manuel.reithuber.net>>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-rearrange
 *
 * Rearrange spreads two input channels to two channels of a 5.1 or 7.1 signal.
 * It therefore allows you to send more than one stereo signals to multichannel
 * sound cards.
 *
 * Keep in mind that some audio backends do up-/downmixing if the signal's
 * channel count doesn't match the channel count of the audio device
 *
 * <refsect2>
 * <title>Example launch lines</title>
 * |[
 * gst-launch -v -m audiotestsrc ! rearrange channels=4 pos=1 ! alsasink
 * ]|
 * Plays a sine wave to the rear channels of a 4-channel output stream
 *
 * |[
 * gst-launch audiotestsrc freq=1200 ! rearrange name=foo channels=4 ! adder name=mix ! alsasink  audiotestsrc freq=444 ! rearrange name=bar channels=4 pos=1 ! mix.
 * ]|
 * Plays a 1200Hz wave at the front and a 444Hz sine at the rear channels of a 4-channel output stream
 *
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/audio/multichannel.h>

#include "gstrearrange.h"

GST_DEBUG_CATEGORY_STATIC (gst_rearrange_debug);
#define GST_CAT_DEFAULT gst_rearrange_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_POS,
  PROP_CHANNELS
};

#define SRCCAPS \
	"audio/x-raw-int," \
		"width = (int) 16," \
		"depth = (int) 16," \
		"endianness = (int) BYTE_ORDER," \
		"channels = (int) {2,4,6,8}," \
		"rate = (int) [1,2147483647];" \
	"audio/x-raw-float," \
		"width = (int) {32, 64}," \
		"endianness = (int) BYTE_ORDER," \
		"channels = (int) {2,4,6,8}," \
		"rate = (int) [1,2147483647]"


#define SINKCAPS \
	"audio/x-raw-int," \
		"width = (int) 16," \
		"depth = (int) 16," \
		"endianness = (int) BYTE_ORDER," \
		"channels = (int) {1,2}," \
		"rate = (int) [1, 2147483647];" \
	"audio/x-raw-float," \
		"width = (int) {32, 64}," \
		"endianness = (int) BYTE_ORDER," \
		"channels = (int) {1,2}," \
		"rate = (int) [1,2147483647]"

static GstAudioChannelPosition _gst_rearrange_positions[8] = {
	GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
	GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
	GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
	GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
	GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
	GST_AUDIO_CHANNEL_POSITION_LFE,
	GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
	GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(SINKCAPS)
	);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(SRCCAPS)
	);

GST_BOILERPLATE (GstReArrange, gst_rearrange, GstElement, GST_TYPE_ELEMENT);

static void gst_rearrange_set_property (GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec);
static void gst_rearrange_get_property (GObject * object, guint prop_id,
	GValue * value, GParamSpec * pspec);

static gboolean gst_rearrange_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_rearrange_chain (GstPad * pad, GstBuffer * buf);

/* GObject vmethod implementations */

static void
gst_rearrange_base_init (gpointer gclass) {
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
	"ReArrange",
	"Re-arrange audio channels",
	"Move the two channels of an incoming stereo signal to two of the other channels (e.g. rear, side or center/lfe)",
	"Manuel Reithuber <gstreamer@manuel.reithuber.net>");

  gst_element_class_add_pad_template (element_class,
	  gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
	  gst_static_pad_template_get (&sink_factory));

}

/* initialize the rearrange's class */
static void
gst_rearrange_class_init (GstReArrangeClass * klass) {
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	gobject_class->set_property = gst_rearrange_set_property;
	gobject_class->get_property = gst_rearrange_get_property;

	g_object_class_install_property (gobject_class, PROP_CHANNELS,
		g_param_spec_uint("channels", "OutputChannels", "Channel count of the output signal",
		2, 8, 8, G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class, PROP_POS,
		g_param_spec_uint("pos", "SignalPos", "Position of the signal (0: front, 1: rear, 2: center/lfe, 3: side)",
		0, 3, 0, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_rearrange_init (GstReArrange * filter, GstReArrangeClass * gclass) {
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
								GST_DEBUG_FUNCPTR(gst_rearrange_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
								GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
							  GST_DEBUG_FUNCPTR(gst_rearrange_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
								GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->outChannels = 8;
  filter->outPos = 0;
}

static void
gst_rearrange_set_property (GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec)
{
	GstReArrange *filter = GST_REARRANGE (object);

	switch (prop_id) {
	case PROP_CHANNELS:
		filter->outChannels = g_value_get_uint(value);
		break;
	case PROP_POS:
		filter->outPos = g_value_get_uint(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
  }
}

static void
gst_rearrange_get_property (GObject * object, guint prop_id,
	GValue * value, GParamSpec * pspec)
{
	GstReArrange *filter = GST_REARRANGE (object);

	switch (prop_id) {
	case PROP_CHANNELS:
		g_print("chanType: %s\n", g_type_name(g_value_array_get_type()));
		g_value_set_uint(value, filter->outChannels);
		break;
	case PROP_POS:
		g_print("posType: %s\n", g_type_name(g_value_array_get_type()));
		g_value_set_uint(value, filter->outPos);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_rearrange_set_caps (GstPad * pad, GstCaps * caps)
{
  GstReArrange *filter;
  GstPad *otherpad;

  filter = GST_REARRANGE (gst_pad_get_parent (pad));
  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

/**
 * Copies the in buffer's caps and changes the channel parameter according
 * to the "channels" property
 * \param sinkCaps Input capabilities (those of the input buffer)
 * \param channels Output channels
 * \note this function is called by gst_rearrange_chain()
 */
GstCaps* gst_rearrange_set_buffer_caps (GstCaps *sinkCaps, int channels) {
	GstCaps *rc = 0;

	GstStructure *struc = gst_structure_copy(gst_caps_get_structure(sinkCaps, 0));

	gst_structure_set (struc, "channels", G_TYPE_INT, channels, NULL);

	gst_audio_set_channel_positions(struc, _gst_rearrange_positions);

	rc = gst_caps_new_empty();
	gst_caps_append_structure(rc, struc);

	return rc;
}

gint gst_rearrange_get_caps_int(GstCaps *sinkCaps, const char *field) {
	gint rc = 0;
	GstStructure *struc = gst_caps_get_structure(sinkCaps, 0);
	if (!gst_structure_get_int(struc, field, &rc)) {
		gchar *capString = gst_caps_to_string(sinkCaps);
		g_print("Problem getting the cap '%s' (caps: '%s')\n", field, capString);
		g_free(capString);
	}

	return rc;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_rearrange_chain (GstPad * pad, GstBuffer * buf)
{
	// get input buffer info
	int width = gst_rearrange_get_caps_int(gst_buffer_get_caps(buf), "width")/8;
	int inChannels = gst_rearrange_get_caps_int(gst_buffer_get_caps(buf), "channels");

	GstReArrange *filter = GST_REARRANGE (GST_OBJECT_PARENT (pad));

	GstBuffer *tgtBuf = gst_buffer_new_and_alloc(buf->size*(filter->outChannels/inChannels));

	// target caps
	GstCaps *tgtCaps = gst_rearrange_set_buffer_caps(gst_buffer_get_caps(buf), filter->outChannels);
	gst_buffer_set_caps(tgtBuf, tgtCaps);
	gst_caps_unref(tgtCaps);

	gint8 *pSrc = buf->data, *pTgt = tgtBuf->data;

	int curByte = 0, fromByte = 2*filter->outPos*width;
	int toByte = fromByte + (width*2); // two channels

	gint8 *pTmp = 0;
	while (pTgt < tgtBuf->data + tgtBuf->size) {
		if (curByte >= fromByte && curByte < toByte) {
			if (inChannels == 1) {
				if (curByte == fromByte) pTmp = pSrc;
				if (curByte == fromByte+width) pSrc = pTmp;
			}
			*pTgt++ = *pSrc++;
		}
		else *pTgt++ = 0;
		curByte = (curByte+1) % (filter->outChannels*width);
	}

	return gst_pad_push(filter->srcpad, tgtBuf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
rearrange_init (GstPlugin * rearrange)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template rearrange' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_rearrange_debug, "rearrange",
	  0, "Template rearrange");

  return gst_element_register (rearrange, "rearrange", GST_RANK_NONE,
	  GST_TYPE_REARRANGE);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "rearrange"
#endif

/* gstreamer looks for this structure to register rearranges
 *
 * exchange the string 'Template rearrange' with your rearrange description
 */
GST_PLUGIN_DEFINE (
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	"rearrange",
	"Channel rearrangement plugin",
	rearrange_init,
	VERSION,
	"LGPL",
	"GStreamer",
	"http://gstreamer.net/"
)
