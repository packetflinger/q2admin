/**
 * Q2Admin
 * Profiling macros, only really used for development
 */

#pragma once

#define INITPERFORMANCE(instance) unsigned long performancetimer##instance
#define INITPERFORMANCE_2(instance) \
            unsigned long performancetimer##instance; \
            static unsigned long totalperformancetimer##instance = 0; \
            static int countperformancetimer##instance = 0

#define STARTPERFORMANCE(instance) \
            if(isLogEvent(LT_PERFORMANCEMONITOR)) \
            { \
                performancetimer##instance = clock(); \
            }

#define STOPPERFORMANCE(instance, function, client, ent) \
            if(isLogEvent(LT_PERFORMANCEMONITOR)) \
            { \
                performancetimer##instance = clock() - performancetimer##instance; \
                if(performancetimer##instance) \
                { \
                    logEvent(LT_PERFORMANCEMONITOR, client, ent, function, 0, (double)performancetimer##instance / CLOCKS_PER_SEC); \
                } \
            }

#define STOPPERFORMANCE_2(instance, function, client, ent) \
            if(isLogEvent(LT_PERFORMANCEMONITOR)) \
            { \
                totalperformancetimer##instance += clock() - performancetimer##instance; \
                countperformancetimer##instance++; \
                if(countperformancetimer##instance >= 100) \
                { \
                    if(totalperformancetimer##instance) \
                    { \
                        logEvent(LT_PERFORMANCEMONITOR, client, ent, function, 0, (double)totalperformancetimer##instance / (100.0 * CLOCKS_PER_SEC)); \
                    } \
                    totalperformancetimer##instance = 0; \
                    countperformancetimer##instance = 0; \
                } \
            }
