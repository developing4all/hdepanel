/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@hosting4all.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */
#ifndef ANIMATIONUTILS_H
#define ANIMATIONUTILS_H

namespace AnimationUtils
{

template<class T>
T animate(T currentPosition, T targetPosition, T step, bool& needAnotherStep)
{
	T result = currentPosition;
	if(result < targetPosition)
	{
		result += step;
		if(result > targetPosition)
			result = targetPosition;
		else
			needAnotherStep = true;
	}
	else
	{
		result -= step;
		if(result < targetPosition)
			result = targetPosition;
		else
			needAnotherStep = true;
	}
	return result;
}

template<class T>
T animateExponentially(T currentPosition, T targetPosition, qreal factor, T minimalStep, bool& needAnotherStep)
{
	T step = static_cast<T>(abs(targetPosition - currentPosition)*factor);
	if(step < minimalStep)
		step = minimalStep;
	return animate(currentPosition, targetPosition, step, needAnotherStep);
}

}

#endif
