#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void flatten_ring_buffer(uint8_t* buffer, uint8_t end_index, uint16_t sample_count)
{
    uint8_t* buffer_copy = malloc(sample_count*sizeof(uint8_t));   
    memcpy(buffer_copy, buffer, sample_count*sizeof(uint8_t));
    uint16_t position = end_index;
    uint16_t i;
    for(i = 0; i < sample_count; i++)
    {
        /*
        buffer[i] = buffer_copy[position];
        position++;
        if(position >= sample_count) 
            position = 0;
        */
        if(i < end_index)
            buffer[sample_count - end_index + i] = buffer_copy[i];  
        else
            buffer[i-end_index] = buffer_copy[i];
    }
    //for(i = 0; i < sample_count/2; i++) buffer[sample_count/2 + i] =  buffer_copy[end_index - sample_count/2 + i];
    free(buffer_copy);
}