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

void Worker::stream_data(UNIT * unit)
{
  BUFFER_INFO buffer_info;
  buffer_info.unit = new UNIT[_UNITCOUNT_];
  uint32_t    sampleCount = 10000;
  uint32_t    sampleInterval = 10;

  for(int16_t u = 0; u < _UNITCOUNT_; u++)
    buffer_info.unit[u] = unit[u];

  for(int16_t u = 0; u < _UNITCOUNT_; u++)
    {
      for (int ch = 0; ch < buffer_info.unit[u].channelCount; ch++)
        {
          if(buffer_info.unit[u].channelSettings[ch].enabled)
            {
              buffer_info.unit[u].channelSettings[ch].driver_buffer = (int16_t*) calloc(sampleCount, sizeof(int16_t));

              ps4000aSetDataBuffer(buffer_info.unit[u].handle,
                                   (PS4000A_CHANNEL)ch,
                                   buffer_info.unit[u].channelSettings[ch].driver_buffer,
                                   sampleCount,
                                   0,
                                   PS4000A_RATIO_MODE_NONE);

              buffer_info.unit[u].channelSettings[ch].app_buffer = (int16_t*) calloc(sampleCount, sizeof(int16_t));
              buffer_info.unit[u].channelSettings[ch].bufferEnabled = true;
            }
        }
    }

  g_streamIsRunning = true;

  for(int16_t u = 0; u < _UNITCOUNT_; u++)
  ps4000aRunStreaming(buffer_info.unit[u].handle,
                      &sampleInterval,
                      PS4000A_US,
                      0,//preTrigger
                      0,//postTrigger
                      0,//autostop
                      1,//downsampleRatio
                      PS4000A_RATIO_MODE_NONE,
                      sampleCount);


  FILE * file_ptr = fopen("data/stream.txt", "w");
  for(int16_t u = 0; u < _UNITCOUNT_; u++)
    for(int ch = 0; ch < buffer_info.unit[u].channelCount; ch++)
      fprintf(file_ptr, buffer_info.unit[u].channelSettings[ch].enabled?"%d\t":"",
              buffer_info.unit[u].channelSettings[ch].mode);
  fprintf(file_ptr, "\n");


  do
    {
      g_ready = 0;

      for(int16_t u = 0; u < _UNITCOUNT_; u++)
        ps4000aGetStreamingLatestValues(buffer_info.unit[u].handle,
                                        callback,
                                        &buffer_info);

      if(g_ready && g_sampleCount > 0)
        {
          double x,y,z0,z1,z2,z3,z4,z5,f0,f1;
          for(int i = g_startIndex; i < (int32_t)(g_startIndex + g_sampleCount); i++)
            {
              for(int16_t u = 0; u < _UNITCOUNT_; u++)
                {
                  for(int ch = 0; ch < buffer_info.unit[u].channelCount; ch++)
                    {
                      if(buffer_info.unit[u].channelSettings[ch].enabled)
                        {
                          double  value = adc_to_voltage(buffer_info.unit[u].channelSettings[ch].range,
                                                         buffer_info.unit[u].maxSampleValue,
                                                         buffer_info.unit[u].channelSettings[ch].app_buffer[i]);

                          switch(buffer_info.unit[u].channelSettings[ch].mode)
                            {
                            case X : x  = value; break;
                            case Y : y  = value; break;
                            case Z0: z0 = value; break;
                            case Z1: z1 = value; break;
                            case Z2: z2 = value; break;
                            case Z3: z3 = value; break;
                            case Z4: z4 = value; break;
                            case Z5: z5 = value; break;
                            case F0: f0 = value; break;
                            case F1: f1 = value; break;
                            default: break;
                            }
                          fprintf(file_ptr, "%f\t", value);
                        }
                    }
                }
              emit(data(x,y,z0,z1,z2,z3,z4,z5,f0,f1));
              fprintf(file_ptr,"\n");
            }
        }
    }
  while (g_streamIsRunning);

  for(int16_t u = 0; u < _UNITCOUNT_; u++)
    ps4000aStop(buffer_info.unit[u].handle);
  emit(unit_stopped_signal());

  for(int16_t u = 0; u < _UNITCOUNT_; u++)
    {
      for (int ch = 0; ch < buffer_info.unit[u].channelCount; ch++)
        {
          if(buffer_info.unit[u].channelSettings[ch].bufferEnabled)
            {
              free(buffer_info.unit[u].channelSettings[ch].driver_buffer);
              free(buffer_info.unit[u].channelSettings[ch].app_buffer);
              buffer_info.unit[u].channelSettings[ch].bufferEnabled = false;
            }
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
      for(int16_t u = 0; u < _UNITCOUNT_; u++)
        {
          for (int ch = 0; ch < buffer_info->unit[u].channelCount; ch++)
            {
              if (buffer_info->unit[u].channelSettings[ch].bufferEnabled)
                {
                  memcpy(&buffer_info->unit[u].channelSettings[ch].app_buffer[startIndex],
                         &buffer_info->unit[u].channelSettings[ch].driver_buffer[startIndex],
                         noOfSamples * sizeof(int16_t));
                }
            }
        }
      g_ready = true;
    }
}

