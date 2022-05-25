#include <QApplication>
#include <QThread>
#include <cstdlib>
#include <cstring>
#include <window.hpp>
#include <cstdio>


Worker::Worker()
{
  voltages = {10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0, 50000.0};
}


double Worker::adc_to_voltage(int range, int16_t maxBits, int16_t bits)
{
  return ((double)bits/(double)maxBits)*voltages[range];
}

void Worker::stream_data(UNIT u)
{
  BUFFER_INFO buffer_info;
  uint32_t    sampleCount = 10000;
  uint32_t    sampleInterval = 10;
  int16_t *   driver_buffer[PS4000A_MAX_CHANNELS];
  int16_t *   app_buffer[PS4000A_MAX_CHANNELS];

  buffer_info.unit            = u;
  buffer_info.driver_buffers  = driver_buffer;
  buffer_info.app_buffers     = app_buffer;




  for (int ch = 0; ch < buffer_info.unit.channelCount; ch++)
    {
      if(buffer_info.unit.channelSettings[ch].enabled)
        {
          driver_buffer[ch] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

          ps4000aSetDataBuffer(buffer_info.unit.handle,
                               (PS4000A_CHANNEL)ch,
                               driver_buffer[ch],
                               sampleCount,
                               0,
                               PS4000A_RATIO_MODE_NONE);

          app_buffer[ch] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
          buffer_info.unit.channelSettings[ch].bufferEnabled = true;
        }
    }

  g_streamIsRunning = true;

  ps4000aRunStreaming(buffer_info.unit.handle,
                      &sampleInterval,
                      PS4000A_US,
                      0,//preTrigger
                      0,//postTrigger
                      0,//autostop
                      1,//downsampleRatio
                      PS4000A_RATIO_MODE_NONE,
                      sampleCount);


  FILE * file_ptr = fopen("data/stream.txt", "w");
  for(int ch = 0; ch < buffer_info.unit.channelCount; ch++)
    fprintf(file_ptr, buffer_info.unit.channelSettings[ch].enabled?"%d\t":"",
            buffer_info.unit.channelSettings[ch].mode);
  fprintf(file_ptr, "\n");


  do
    {
      g_ready = 0;

      ps4000aGetStreamingLatestValues(buffer_info.unit.handle,
                                      callback,
                                      &buffer_info);

      if(g_ready && g_sampleCount > 0)
        {
          double x,y,z0,z1,z2,z3,z4,z5;
          for(int i = g_startIndex; i < (int32_t)(g_startIndex + g_sampleCount); i++)
            {
              for(int ch = 0; ch < buffer_info.unit.channelCount; ch++)
                {
                  if(buffer_info.unit.channelSettings[ch].enabled)
                    {
                      double  value = adc_to_voltage(buffer_info.unit.channelSettings[ch].range,
                                                     buffer_info.unit.maxSampleValue,
                                                     buffer_info.app_buffers[ch][i]);

                      switch(buffer_info.unit.channelSettings[ch].mode)
                        {
                        case X : x  = value; break;
                        case Y : y  = value; break;
                        case Z0: z0 = value; break;
                        case Z1: z1 = value; break;
                        case Z2: z2 = value; break;
                        case Z3: z3 = value; break;
                        case Z4: z4 = value; break;
                        case Z5: z5 = value; break;
                        default: break;
                        }
                      fprintf(file_ptr, "%f\t", value);
                    }
                }
              emit(data(x,y,z0,z1,z2,z3,z4,z5));
              fprintf(file_ptr,"\n");
            }
        }
    }
  while (g_streamIsRunning);

  ps4000aStop(buffer_info.unit.handle);
  emit(unit_stopped_signal());

  for (int ch = 0; ch < buffer_info.unit.channelCount; ch++)
    {
      if(buffer_info.unit.channelSettings[ch].bufferEnabled)
        {
          free(driver_buffer[ch]);
          free(app_buffer[ch]);
          buffer_info.unit.channelSettings[ch].bufferEnabled = false;
        }
    }
  fclose(file_ptr);
}




void Worker::callback(
                               int16_t      handle,
                               int32_t      noOfSamples,
                               uint32_t     startIndex,
                               int16_t      overflow,
                               uint32_t     triggerAt,
                               int16_t      triggered,
                               int16_t      autoStop,
                               void	*       pParameter)
{
  BUFFER_INFO * buffer_info = (BUFFER_INFO *)pParameter;

  g_sampleCount = noOfSamples;
  g_startIndex  = startIndex;

  if (noOfSamples)
    {
      for (int ch = 0; ch < buffer_info->unit.channelCount; ch++)
        {
          if (buffer_info->unit.channelSettings[ch].bufferEnabled)
            {
              memcpy(&buffer_info->app_buffers[ch][startIndex],
                     &buffer_info->driver_buffers[ch][startIndex],
                     noOfSamples * sizeof(int16_t));
            }
        }
      g_ready = true;
    }
}

