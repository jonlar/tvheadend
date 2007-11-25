/*
 *  tvheadend, MPEG transport stream muxer
 *  Copyright (C) 2007 Andreas �man
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TSMUX_H
#define TSMUX_H

th_muxer_t *ts_muxer_init(th_subscription_t *s, th_mux_output_t *cb,
			  void *opaque, int flags);

void ts_muxer_deinit(th_muxer_t *tm, th_subscription_t *s);

void ts_muxer_play(th_muxer_t *tm, int64_t toffset);

void ts_muxer_pause(th_muxer_t *tm);

#endif /* TSMUX_H */