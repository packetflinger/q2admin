/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#define profile_init(instance) unsigned long performancetimer##instance = 0;
#define profile_init_2(instance) \
            unsigned long performancetimer##instance = 0; \
            static unsigned long totalperformancetimer##instance = 0; \
            static int countperformancetimer##instance = 0

#define profile_start(instance) \
            if(isLogEvent(LT_PERFORMANCEMONITOR)) \
            { \
                performancetimer##instance = clock(); \
            }

#define profile_stop(instance, function, client, ent) \
            if(isLogEvent(LT_PERFORMANCEMONITOR)) \
            { \
                performancetimer##instance = clock() - performancetimer##instance; \
                if(performancetimer##instance) \
                { \
                    logEvent(LT_PERFORMANCEMONITOR, client, ent, function, 0, (double)performancetimer##instance / CLOCKS_PER_SEC, false); \
                } \
            }

#define profile_stop_2(instance, function, client, ent) \
            if(isLogEvent(LT_PERFORMANCEMONITOR)) \
            { \
                totalperformancetimer##instance += clock() - performancetimer##instance; \
                countperformancetimer##instance++; \
                if(countperformancetimer##instance >= 100) \
                { \
                    if(totalperformancetimer##instance) \
                    { \
                        logEvent(LT_PERFORMANCEMONITOR, client, ent, function, 0, (double)totalperformancetimer##instance / (100.0 * CLOCKS_PER_SEC), false); \
                    } \
                    totalperformancetimer##instance = 0; \
                    countperformancetimer##instance = 0; \
                } \
            }
