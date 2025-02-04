/*
 *
 * This piece of software is an implementation of the Generic Stream
 * Encapsulation (GSE) standard defined by ETSI for Linux (or other
 * Unix-compatible OS). The library may be used to add GSE
 * encapsulation/de-encapsulation capabilities to an application.
 *
 *
 * Copyright © 2016 TAS
 *
 *
 * This file is part of the GSE library.
 *
 *
 * The GSE library is free software : you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 *
 */

/****************************************************************************/
/**
 *   @file          virtual_fragment.c
 *
 *          Project:     GSE LIBRARY
 *
 *          Company:     THALES ALENIA SPACE
 *
 *          Module name: VIRTUAL FRAGMENT
 *
 *   @brief         Virtual buffer and fragments mangement
 *
 *   @author        Julien BERNARD / Viveris Technologies
 *   @author        Audric Schiltknecht / Viveris Technologies
 *   @author        Joaquin Muguerza / Viveris Technologies
 *
 */
/****************************************************************************/

#include "virtual_fragment.h"

#include <stdlib.h>
#include <assert.h>


/****************************************************************************
 *
 *   PROTOTYPES OF PRIVATE FUNCTIONS
 *
 ****************************************************************************/

/**
 *  @brief   Get the number of fragments in a virtual buffer related to a
 *           virtual fragment
 *
 *  @param   vfrag The virtual fragment depending on the virtual buffer
 *
 *  @return        Number of fragments on success,
 *                 -1 on failure
 */
static int gse_get_vfrag_nbr(gse_vfrag_t *vfrag);

/**
 *  @brief   Create a virtual buffer
 *
 *  @param   vbuf    The virtual buffer
 *  @param   length  The virtual buffer length, in bytes
 *
 *  @return
 *                   - success/informative code among:
 *                     - \ref GSE_STATUS_OK
 *                   - warning/error code among:
 *                     - \ref GSE_STATUS_MALLOC_FAILED
 */
static gse_status_t gse_create_vbuf(gse_vbuf_t **vbuf, size_t length);

/**
 *  @brief    Free a virtual buffer
 *
 *  @param   vbuf  The virtual buffer that will be destroyed
 *
 *  @return
 *                 - success/informative code among:
 *                   - \ref GSE_STATUS_OK
 *                 - warning/error code among:
 *                   - \ref GSE_STATUS_FRAG_NBR
 */
static gse_status_t gse_free_vbuf(gse_vbuf_t *vbuf);

/****************************************************************************
 *
 *   PUBLIC FUNCTIONS
 *
 ****************************************************************************/

gse_status_t gse_create_vfrag(gse_vfrag_t **vfrag, size_t max_length,
                              size_t head_offset, size_t trail_offset)
{
  gse_status_t status = GSE_STATUS_OK;

  gse_vbuf_t *vbuf;
  size_t length_buf;

  if(vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  /* The length of the buffer contining the fragment is the fragment length
     plus the offsets */
  length_buf = max_length + head_offset + trail_offset;
  if(length_buf == 0)
  {
    status = GSE_STATUS_BUFF_LENGTH_NULL;
    goto error;
  }

  status = gse_create_vbuf(&vbuf, length_buf);
  if(status != GSE_STATUS_OK)
  {
    goto error;
  }

  *vfrag = malloc(sizeof(gse_vfrag_t));
  if(*vfrag == NULL)
  {
    status = GSE_STATUS_MALLOC_FAILED;
    goto free_vbuf;
  }
  (*vfrag)->vbuf = vbuf;
  (*vfrag)->start = ((*vfrag)->vbuf->start + head_offset),
  (*vfrag)->length = max_length;
  (*vfrag)->end = (*vfrag)->start + (*vfrag)->length;
  assert(((*vfrag)->end) <= ((*vfrag)->vbuf->end));
  assert(((*vfrag)->end) >= ((*vfrag)->vbuf->start));
  if(((*vfrag)->end) > ((*vfrag)->vbuf->end) ||
     ((*vfrag)->end) < ((*vfrag)->vbuf->start))
  {
    status = GSE_STATUS_INTERNAL_ERROR;
    goto free_vfrag;
  }
  vbuf->vfrag_count++;

  return status;
free_vfrag:
  free(*vfrag);
free_vbuf:
  gse_free_vbuf(vbuf);
error:
  if(vfrag != NULL)
  {
    *vfrag = NULL;
  }
  return status;
}

gse_status_t gse_create_vfrag_with_data(gse_vfrag_t **vfrag, size_t max_length,
                                        size_t head_offset, size_t trail_offset,
                                        unsigned char const *data,
                                        size_t data_length)
{
  gse_status_t status = GSE_STATUS_OK;

  status = gse_create_vfrag(vfrag, max_length, head_offset, trail_offset);
  if(status != GSE_STATUS_OK)
  {
    goto error;
  }

  status = gse_copy_data((*vfrag), data, data_length);
  if(status != GSE_STATUS_OK)
  {
    goto free_vfrag;
  }

  return status;
free_vfrag:
  gse_free_vfrag(vfrag);
error:
  return status;
}

gse_status_t gse_copy_data(gse_vfrag_t *vfrag, unsigned char const *data,
                           size_t data_length)
{
  gse_status_t status = GSE_STATUS_OK;

  if(vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  if(data == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  /* If there is more than one virtual fragment in buffer, don't overwrite data */
  if(gse_get_vfrag_nbr(vfrag) > 1)
  {
    status = GSE_STATUS_MULTIPLE_VBUF_ACCESS;
    goto error;
  }
  /* Check if there is enough space in buffer */
  if((vfrag->length) < data_length)
  {
    status = GSE_STATUS_DATA_TOO_LONG;
    goto error;
  }
  /* Copy data in vfrag and update vfrag structure */
  memcpy(vfrag->start, data, data_length);
  vfrag->length = data_length;
  vfrag->end = vfrag->start + vfrag->length;
  assert((vfrag->end) <= (vfrag->vbuf->end));
  assert((vfrag->end) >= (vfrag->vbuf->start));
  if((vfrag->end) > (vfrag->vbuf->end) ||
     (vfrag->end) < (vfrag->vbuf->start))
  {
    status = GSE_STATUS_INTERNAL_ERROR;
    goto error;
  }

error:
  return status;
}

gse_status_t gse_create_vfrag_from_buf(gse_vfrag_t **vfrag, unsigned char *buffer,
                                       unsigned int head_offset, unsigned int trail_offset,
                                       unsigned int data_length)
{
  int status = GSE_STATUS_OK;
  gse_vbuf_t *vbuf;           
                               
  if(vfrag == NULL || buffer == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  vbuf = malloc(sizeof(gse_vbuf_t));
  if(vbuf == NULL)
  {
    status = GSE_STATUS_MALLOC_FAILED;
    goto error;
  }

  vbuf->start = buffer;
  vbuf->length = head_offset + data_length + trail_offset;
  vbuf->end = vbuf->start + vbuf->length;
  vbuf->vfrag_count = 0;

  *vfrag = malloc(sizeof(gse_vfrag_t));
  if(*vfrag == NULL)
  {
    status = GSE_STATUS_MALLOC_FAILED;
    goto free_vbuf;
  }

  (*vfrag)->vbuf = vbuf;
  (*vfrag)->start = ((*vfrag)->vbuf->start + head_offset),
  (*vfrag)->length = data_length;
  (*vfrag)->end = (*vfrag)->start + (*vfrag)->length;
  assert(((*vfrag)->end) <= ((*vfrag)->vbuf->end));
  assert(((*vfrag)->end) >= ((*vfrag)->vbuf->start));
  assert((vbuf->end - (*vfrag)->end) == (int)trail_offset);
  if(((*vfrag)->end) > ((*vfrag)->vbuf->end) ||
     ((*vfrag)->end) < ((*vfrag)->vbuf->start) ||
     (vbuf->end - (*vfrag)->end) != (int)trail_offset)
  {
    status = GSE_STATUS_INTERNAL_ERROR;
    goto free_vfrag;
  }
  vbuf->vfrag_count++;

  return status;
free_vfrag:
  free(*vfrag);
free_vbuf:
  free(vbuf);
error:
  if(vfrag != NULL)
  {
    *vfrag = NULL;
  }
  return status;
}

gse_status_t gse_allocate_vfrag(gse_vfrag_t **vfrag, int alloc_vbuf)
{
  int status = GSE_STATUS_OK;
  gse_vbuf_t *vbuf = NULL;
    
  if(vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  if (alloc_vbuf)
  {
    vbuf = malloc(sizeof(gse_vbuf_t));
    if(vbuf == NULL)
    {
      status = GSE_STATUS_MALLOC_FAILED;
      goto error;
    }

    vbuf->vfrag_count = 0;
  }
  
  *vfrag = malloc(sizeof(gse_vfrag_t));
  if(*vfrag == NULL)
  {
    status = GSE_STATUS_MALLOC_FAILED;
    goto free_vbuf;
  }
    
  (*vfrag)->vbuf = vbuf;
  
  return status;
    
free_vbuf:
  free(vbuf);
error:
  if(vfrag != NULL)
  {
    *vfrag = NULL;
  }
  return status;
}

gse_status_t gse_affect_buf_vfrag(gse_vfrag_t *vfrag, unsigned char *buffer,
                                  unsigned int head_offset, unsigned int trail_offset,
                                  unsigned int data_length)
{
  int status = GSE_STATUS_OK;
  gse_vbuf_t *vbuf;           
                               
  if(vfrag == NULL || buffer == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  vbuf = vfrag->vbuf;
  if(vbuf == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  vbuf->start = buffer;
  vbuf->length = head_offset + data_length + trail_offset;
  vbuf->end = vbuf->start + vbuf->length;
  vbuf->vfrag_count = 0;

  vfrag->start = (vbuf->start + head_offset);
  vfrag->length = data_length;
  vfrag->end = vfrag->start + vfrag->length;
  assert((vfrag->end) <= (vfrag->vbuf->end));
  assert((vfrag->end) >= (vfrag->vbuf->start));
  assert((vbuf->end - vfrag->end) == (int)trail_offset);
  if((vfrag->end) > (vfrag->vbuf->end) ||
     (vfrag->end) < (vfrag->vbuf->start) ||
     (vbuf->end - vfrag->end) != (int)trail_offset)
  {
    status = GSE_STATUS_INTERNAL_ERROR;
    goto error;
  }
  vbuf->vfrag_count++;

error:
  return status;
}

gse_status_t gse_free_vfrag(gse_vfrag_t **vfrag)
{
  gse_status_t status;

  if(vfrag == NULL || *vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  if(gse_get_vfrag_nbr(*vfrag) <= 0)
  {
    status = GSE_STATUS_FRAG_NBR;
    goto error;
  }

  (*vfrag)->vbuf->vfrag_count--;

  if(gse_get_vfrag_nbr(*vfrag) == 0)
  {
    status = gse_free_vbuf((*vfrag)->vbuf);
    if(status != GSE_STATUS_OK)
    {
      goto free_vfrag;
    }
  }

  status = GSE_STATUS_OK;

free_vfrag:
  free(*vfrag);
  *vfrag = NULL;
error:
  return status;
}

gse_status_t gse_free_vfrag_no_alloc(gse_vfrag_t **vfrag, int reset, int free_vbuf)
{
  gse_status_t status = GSE_STATUS_OK;

  if(vfrag == NULL || *vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  if(reset)
  {
    if((*vfrag)->vbuf->vfrag_count == 0)
    {
      status = GSE_STATUS_FRAG_NBR;
      goto error;
    }
 
    (*vfrag)->vbuf->vfrag_count--;
  }
  else
  {
    if(free_vbuf)
    {
      free((*vfrag)->vbuf);
      (*vfrag)->vbuf = NULL;
    }
   
    free(*vfrag);
    *vfrag = NULL;
  }
   
error:
  return status;
}


gse_status_t gse_duplicate_vfrag(gse_vfrag_t **vfrag, gse_vfrag_t *father,
                                 size_t length)
{
  gse_status_t status = GSE_STATUS_OK;

  if(father == NULL || vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  /* If the father is empty it is not duplicated */
  if(father->length == 0)
  {
    status = GSE_STATUS_EMPTY_FRAG;
    goto error;
  }

  /* There can be only two accesses to a virtual buffer to avoid multiple
   * accesses from duplicated virtual fragments */
  if(gse_get_vfrag_nbr(father) >= 2)
  {
    status = GSE_STATUS_FRAG_NBR;
    goto error;
  }

  *vfrag = malloc(sizeof(gse_vfrag_t));
  if(*vfrag == NULL)
  {
    status = GSE_STATUS_MALLOC_FAILED;
    goto error;
  }

  (*vfrag)->vbuf = father->vbuf;
  (*vfrag)->start = father->start;
  (*vfrag)->length = MIN(length, father->length);

  (*vfrag)->end = (*vfrag)->start + (*vfrag)->length;
  assert(((*vfrag)->end) <= ((*vfrag)->vbuf->end));
  assert(((*vfrag)->end) >= ((*vfrag)->vbuf->start));
  if(((*vfrag)->end) > ((*vfrag)->vbuf->end) ||
     ((*vfrag)->end) < ((*vfrag)->vbuf->start))
  {
    status = GSE_STATUS_INTERNAL_ERROR;
    goto free_vfrag;
  }
  (*vfrag)->vbuf->vfrag_count++;

  return status;
free_vfrag:
  free(*vfrag);
error:
  if(vfrag != NULL)
  {
    *vfrag = NULL;
  }
  return status;
}

gse_status_t gse_duplicate_vfrag_no_alloc(gse_vfrag_t **vfrag, gse_vfrag_t *father,
                                          size_t length)
{
  gse_status_t status = GSE_STATUS_OK;

  if(father == NULL || vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  if(father->vbuf == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  /* If the father is empty it is not duplicated */
  if(father->length == 0)
  {
    status = GSE_STATUS_EMPTY_FRAG;
    goto error;
  }

  /* There can be only two accesses to a virtual buffer to avoid multiple
   * accesses from duplicated virtual fragments */
  if(gse_get_vfrag_nbr(father) >= 2)
  {
    status = GSE_STATUS_FRAG_NBR;
    goto error;
  }

  (*vfrag)->vbuf = father->vbuf;
  (*vfrag)->start = father->start;
  (*vfrag)->length = MIN(length, father->length);

  (*vfrag)->end = (*vfrag)->start + (*vfrag)->length;
  assert(((*vfrag)->end) <= ((*vfrag)->vbuf->end));
  assert(((*vfrag)->end) >= ((*vfrag)->vbuf->start));
  if(((*vfrag)->end) > ((*vfrag)->vbuf->end) ||
     ((*vfrag)->end) < ((*vfrag)->vbuf->start))
  {
    status = GSE_STATUS_INTERNAL_ERROR;
    goto error;
  }
  (*vfrag)->vbuf->vfrag_count++;

error:
  return status;
}

gse_status_t gse_shift_vfrag(gse_vfrag_t *vfrag, int start_shift, int end_shift)
{
  gse_status_t status = GSE_STATUS_OK;

  if(vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  /* Check if pointer will not be outside buffer */
  if(((vfrag->start + start_shift) < vfrag->vbuf->start) ||
     ((vfrag->start + start_shift) > vfrag->vbuf->end))
  {
    status = GSE_STATUS_PTR_OUTSIDE_BUFF;
    goto error;
  }

  if(((vfrag->end + end_shift) < vfrag->vbuf->start) ||
     ((vfrag->end + end_shift) > vfrag->vbuf->end))
  {
    status = GSE_STATUS_PTR_OUTSIDE_BUFF;
    goto error;
  }

  if((vfrag->start + start_shift) > (vfrag->end + end_shift))
  {
    status = GSE_STATUS_FRAG_PTRS;
    goto error;
  }

  vfrag->start += start_shift;
  vfrag->end += end_shift;
  vfrag->length += end_shift - start_shift;

error:
  return status;
}

gse_status_t gse_reset_vfrag(gse_vfrag_t *vfrag, size_t *length,
                             size_t head_offset, size_t trail_offset)
{
  gse_status_t status = GSE_STATUS_OK;

  if(vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  if(vfrag->vbuf->length < (head_offset + trail_offset))
  {
    status = GSE_STATUS_OFFSET_TOO_HIGH;
    goto error;
  }
  vfrag->start = vfrag->vbuf->start + head_offset;
  vfrag->end = vfrag->vbuf->end - trail_offset;
  vfrag->length = vfrag->vbuf->length - head_offset - trail_offset;
  assert((vfrag->end) <= (vfrag->vbuf->end));
  assert((vfrag->end) >= (vfrag->vbuf->start));
  assert((vfrag->start) <= (vfrag->vbuf->end));
  assert((vfrag->start) >= (vfrag->vbuf->start));
  if((vfrag->end) > (vfrag->vbuf->end) ||
     (vfrag->end) < (vfrag->vbuf->start) ||
     (vfrag->start) > (vfrag->vbuf->end) ||
     (vfrag->start) < (vfrag->vbuf->start))
  {
    status = GSE_STATUS_INTERNAL_ERROR;
    goto error;
  }

  *length = vfrag->length;

error:
  return status;
}

unsigned char *gse_get_vfrag_start(gse_vfrag_t *vfrag)
{
  if(vfrag == NULL)
  {
    return NULL;
  }
  return (vfrag->start);
}

size_t gse_get_vfrag_length(gse_vfrag_t *vfrag)
{
  assert(vfrag != NULL);

  return (vfrag->length);
}

gse_status_t gse_set_vfrag_length(gse_vfrag_t *vfrag, size_t length)
{
  gse_status_t status = GSE_STATUS_OK;

  if(vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  if((vfrag->start + length) > vfrag->vbuf->end)
  {
    status = GSE_STATUS_PTR_OUTSIDE_BUFF;
    goto error;
  }

  vfrag->end = vfrag->start + length;
  vfrag->length = length;

error:
  return status;
}

size_t gse_get_vfrag_available_head(gse_vfrag_t *vfrag)
{
  assert(vfrag != NULL);

  return (vfrag->start - vfrag->vbuf->start);
}

size_t gse_get_vfrag_available_trail(gse_vfrag_t *vfrag)
{
  assert(vfrag != NULL);

  return (vfrag->vbuf->end - vfrag->end);
}


gse_status_t gse_reallocate_vfrag(gse_vfrag_t *vfrag, size_t start_offset,
                                  size_t max_length, size_t head_offset,
                                  size_t trail_offset)
{
  gse_status_t status = GSE_STATUS_OK;

  size_t length_buf;
  unsigned char *new_ptr;

  if(vfrag == NULL)
  {
    status = GSE_STATUS_NULL_PTR;
    goto error;
  }

  /* start offset is the start of data, it shall be greater that head_offset */
  if(start_offset < head_offset || start_offset > (head_offset + max_length))
  {
    status = GSE_STATUS_BAD_OFFSETS;
    goto error;
  }

  /* The length of the buffer contining the fragment is the fragment length
     plus the offsets */
  length_buf = max_length + head_offset + trail_offset;
  if(length_buf == 0)
  {
    status = GSE_STATUS_BUFF_LENGTH_NULL;
    goto error;
  }

  /* increase the length of the global buffer */
  new_ptr = calloc(length_buf, sizeof(unsigned char));
  if(new_ptr == NULL)
  {
    status = GSE_STATUS_MALLOC_FAILED;
    goto error;
  }
  /* move the previous data in the new buffer */
  memcpy(new_ptr + start_offset, vfrag->start,
         MIN(max_length + head_offset - start_offset, vfrag->length));

  free(vfrag->vbuf->start);
  /* update the buffer */
  vfrag->vbuf->start = new_ptr;
  /* update the virtual buffer length and end pointer */
  vfrag->vbuf->length = length_buf;
  vfrag->vbuf->end = vfrag->vbuf->start + vfrag->vbuf->length;

  /* update the virtual fragment start and end pointers,
   * length is only modified if new available length is smaller */
  vfrag->start = (vfrag->vbuf->start + start_offset);
  vfrag->length = MIN(vfrag->length, max_length - start_offset);
  vfrag->end = vfrag->start + vfrag->length;

  assert((vfrag->end) <= (vfrag->vbuf->end));
  assert((vfrag->end) >= (vfrag->vbuf->start));
  if((vfrag->end) > (vfrag->vbuf->end) ||
     (vfrag->end) < (vfrag->vbuf->start))
  {
    status = GSE_STATUS_INTERNAL_ERROR;
    goto error;
  }

error:
  return status;

}

/****************************************************************************
 *
 *   PRIVATE FUNCTIONS
 *
 ****************************************************************************/

static int gse_get_vfrag_nbr(gse_vfrag_t *vfrag)
{
  if(vfrag == NULL)
  {
    return -1;
  }
  return(vfrag->vbuf->vfrag_count);
}

static gse_status_t gse_create_vbuf(gse_vbuf_t **vbuf, size_t length)
{
  gse_status_t status = GSE_STATUS_OK;

  assert(vbuf != NULL);

  *vbuf = malloc(sizeof(gse_vbuf_t));
  if(*vbuf == NULL)
  {
    status = GSE_STATUS_MALLOC_FAILED;
    goto error;
  }

  (*vbuf)->start = calloc(length, sizeof(unsigned char));
  if((*vbuf)->start == NULL)
  {
    status = GSE_STATUS_MALLOC_FAILED;
    goto free_vbuf;
  }
  (*vbuf)->length = length;

  (*vbuf)->end = (*vbuf)->start + (*vbuf)->length;
  (*vbuf)->vfrag_count = 0;

  return status;
free_vbuf:
  free(*vbuf);
  *vbuf = NULL;
error:
  return status;
}

static gse_status_t gse_free_vbuf(gse_vbuf_t *vbuf)
{
  gse_status_t status = GSE_STATUS_OK;

  assert(vbuf != NULL);

  /* This function should only be called if there is no more fragment in buffer */
  if(vbuf->vfrag_count != 0)
  {
    status = GSE_STATUS_FRAG_NBR;
    goto error;
  }
  free(vbuf->start);
  free(vbuf);

error:
  return status;
}
