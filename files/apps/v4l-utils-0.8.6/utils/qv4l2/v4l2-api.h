/* v4l2-api: low-level wrapper around v4l2 devices
 *
 * Copyright (C) 2009 Hans Verkuil <hverkuil@xs4all.nl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef V4L2_API_H
#define V4L2_API_H

#include <QString>
#include <linux/videodev2.h>
#include <libv4lconvert.h>

class v4l2
{
public:
	v4l2() : m_fd(-1) {}
	v4l2(v4l2 &old) :
		m_fd(old.m_fd),
		m_device(old.m_device),
		m_useWrapper(old.m_useWrapper),
		m_capability(old.m_capability)
	{}

	bool open(const QString &device, bool useWrapper = true);
	void close();
	int read(unsigned char *p, int size);
	int ioctl(unsigned cmd, void *arg);
	bool ioctl(const QString &descr, unsigned cmd, void *arg);
	void *mmap(size_t length, __off64_t offset);
	int munmap(void *_start, size_t length);

	inline int fd() const { return m_fd; }
	inline bool useWrapper() const { return m_useWrapper; }
	inline __u32 caps() const { return m_capability.capabilities; }
	inline const QString &device() const { return m_device; }
	static QString pixfmt2s(unsigned pixelformat);

	virtual void error(const QString &text);

	bool querycap(v4l2_capability &cap);
	bool queryctrl(v4l2_queryctrl &qc);
	bool querymenu(v4l2_querymenu &qm);
	bool g_tuner(v4l2_tuner &tuner);
	bool g_input(int &input);
	bool s_input(int input);
	bool g_output(int &output);
	bool s_output(int output);
	bool g_audio(v4l2_audio &audio);
	bool s_audio(int input);
	bool g_audout(v4l2_audioout &audout);
	bool s_audout(int output);
	bool s_std(v4l2_std_id std);
	bool g_std(v4l2_std_id &std);
	bool s_dv_preset(__u32 preset);
	bool g_dv_preset(__u32 &preset);
	bool g_frequency(v4l2_frequency &freq);
	bool s_frequency(v4l2_frequency &freq);
	bool s_frequency(int freq);
	bool g_fmt_cap(v4l2_format &fmt);
	bool g_fmt_out(v4l2_format &fmt);
	bool try_fmt(v4l2_format &fmt);
	bool s_fmt(v4l2_format &fmt);
	bool enum_input(v4l2_input &in, bool init = false, int index = 0);
	bool enum_output(v4l2_output &out, bool init = false, int index = 0);
	bool enum_audio(v4l2_audio &audio, bool init = false);
	bool enum_audout(v4l2_audioout &audout, bool init = false);
	bool enum_std(v4l2_standard &std, bool init = false, int index = 0);
	bool enum_dv_preset(v4l2_dv_enum_preset &preset, bool init = false, int index = 0);
	bool enum_fmt_cap(v4l2_fmtdesc &std, bool init = false, int index = 0);
	bool enum_fmt_out(v4l2_fmtdesc &std, bool init = false, int index = 0);
	bool enum_framesizes(v4l2_frmsizeenum &frm, __u32 init_pixfmt = 0, int index = 0);
	bool enum_frameintervals(v4l2_frmivalenum &frm, __u32 init_pixfmt = 0, __u32 w = 0, __u32 h = 0, int index = 0);

	bool reqbufs_mmap_cap(v4l2_requestbuffers &reqbuf, int count = 0);
	bool reqbufs_user_cap(v4l2_requestbuffers &reqbuf, int count = 0);
	bool dqbuf_mmap_cap(v4l2_buffer &buf, bool &again);
	bool dqbuf_user_cap(v4l2_buffer &buf, bool &again);
	bool qbuf(v4l2_buffer &buf);
	bool qbuf_mmap_cap(int index);
	bool qbuf_user_cap(int index, void *ptr, int length);
	bool streamon_cap();
	bool streamoff_cap();

	bool reqbufs_mmap_out(v4l2_requestbuffers &reqbuf, int count = 0);
	bool reqbufs_user_out(v4l2_requestbuffers &reqbuf);
	bool dqbuf_mmap_out(v4l2_buffer &buf);
	bool dqbuf_user_out(v4l2_buffer &buf);
	bool qbuf_mmap_out(int index, int bytesused);
	bool qbuf_user_out(void *ptr, int length);
	bool streamon_out();
	bool streamoff_out();

	bool set_interval(v4l2_fract interval);
	bool get_interval(v4l2_fract &interval);
private:
	void clear() { error(QString()); }

private:
	int 		m_fd;
	QString 	m_device;
	bool 		m_useWrapper;		// true if using the libv4l2 wrappers
	v4l2_capability m_capability;
};

#endif
