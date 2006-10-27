/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include "ompi/communicator/communicator.h"
#include "ompi/win/win.h"
#include "ompi/file/file.h"
#include "ompi/request/request.h"
#include "ompi/errhandler/errhandler.h"
#include "ompi/mpi/f77/fint_2_int.h"


int ompi_errhandler_invoke(ompi_errhandler_t *errhandler, void *mpi_object, 
			   int object_type, int err_code, const char *message)
{
    MPI_Fint fortran_handle, fortran_err_code = OMPI_INT_2_FINT(err_code);
    ompi_communicator_t *comm;
    ompi_win_t *win;
    ompi_file_t *file;
    
    /* If we got no errorhandler, then just invoke errors_abort */
    if (NULL == errhandler) {
	ompi_mpi_errors_are_fatal_comm_handler(NULL, NULL, message);
    }
    
    /* Figure out what kind of errhandler it is, figure out if it's
       fortran or C, and then invoke it */
    
    switch (object_type) {
	case OMPI_ERRHANDLER_TYPE_COMM:
	    comm = (ompi_communicator_t *) mpi_object;
	    if (errhandler->eh_fortran_function) {
		fortran_handle = OMPI_INT_2_FINT(comm->c_f_to_c_index);
		errhandler->eh_fort_fn(&fortran_handle, &fortran_err_code);
	    } else {
		errhandler->eh_comm_fn(&comm, &err_code, message, NULL);
	  }
	  break;

	case OMPI_ERRHANDLER_TYPE_WIN:
	    win = (ompi_win_t *) mpi_object;
	    if (errhandler->eh_fortran_function) {
		fortran_handle = OMPI_INT_2_FINT(win->w_f_to_c_index);
		errhandler->eh_fort_fn(&fortran_handle, &fortran_err_code);
	    } else {
		errhandler->eh_win_fn(&win, &err_code, message, NULL);
	    }
	    break;
	    
	case OMPI_ERRHANDLER_TYPE_FILE:
	    file = (ompi_file_t *) mpi_object;
	    if (errhandler->eh_fortran_function) {
		fortran_handle = OMPI_INT_2_FINT(file->f_f_to_c_index);
		errhandler->eh_fort_fn(&fortran_handle, &fortran_err_code);
	    } else {
		errhandler->eh_file_fn(&file, &err_code, message, NULL);
	    }
	    break;
    }
    
    /* All done */
    return err_code;
}

int ompi_errhandler_request_invoke(int count, 
                                   struct ompi_request_t **requests,
                                   const char *message)
{
    int i, ec;
    ompi_mpi_object_t mpi_object;

    /* Find the first request that has an error.  In an error
       condition, the request will not have been reset back to
       MPI_REQUEST_NULL, so there's no need to cache values from
       before we call ompi_request_test(). */
    for (i = 0; i < count; ++i) {
        if (MPI_REQUEST_NULL != requests[i] &&
            MPI_SUCCESS != requests[i]->req_status.MPI_ERROR) {
            break;
        }
    }
    /* This shouldn't happen */
    if (i >= count) {
        return MPI_SUCCESS;
    }

    ec = ompi_errcode_get_mpi_code(requests[i]->req_status.MPI_ERROR);
    mpi_object = requests[i]->req_mpi_object;
    switch (requests[i]->req_type) {
    case OMPI_REQUEST_PML:
        return ompi_errhandler_invoke(mpi_object.comm->error_handler,
                                      mpi_object.comm,
                                      mpi_object.comm->errhandler_type,
                                      ec, message);
        break;
    case OMPI_REQUEST_IO:
        return ompi_errhandler_invoke(mpi_object.file->error_handler,
                                      mpi_object.file,
                                      mpi_object.file->errhandler_type,
                                      ec, message);
        break;
    case OMPI_REQUEST_WIN:
        return ompi_errhandler_invoke(mpi_object.win->error_handler,
                                      mpi_object.win,
                                      mpi_object.win->errhandler_type,
                                      ec, message);
        break;
    default:
        /* Covers REQUEST_GEN, REQUEST_NULL, REQUEST_MAX */
        return ompi_errhandler_invoke(MPI_COMM_WORLD->error_handler,
                                      MPI_COMM_WORLD,
                                      MPI_COMM_WORLD->errhandler_type,
                                      ec, message);
        break;
    }
}
