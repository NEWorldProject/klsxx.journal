# AppendFile Journal Design Document

## Design Goal

Implement a rotating append only file journal which can record arbitrary serialized binary data spaced with checkpoint
barriers and checkpoint retire messages that facilitate the recovery of program state after an abnormal termination of
the service

## Complexity Requirement

### Time Complexity

Data append, checkpoint append, checkpoint retire: constant on average State recovery: linear of total record size

### RAM Storage Complexity

O(a * checkpoints + b * files) + 4MiB * active files, where a and b are constants determined by the characteristics of
the implementation

### Disk Storage Complexity

Linear on average of total number of bytes all record takes.

## Directory Structure

The directory structure of a journal storage is a flat structure containing a list of journal files. All journal files
must be a regular file with the extension of ".journal" and filename stem of a valid decimal representation of a 64-bit
unsigned integer. All other files that have filenames that do not satisfy the journal file filename requirement will be
silently ignored.

For all journal files under a given storage directory, the 64-bit unsigned integers represented by all stems of the
filenames must be unique, and for all these integers denoted by set S, S must be equal to the set resulted by \[min(S),
max(S)\] intersected with Z. Violation of this rule is considered a detectable error during setup phase of the
implementation and an exception based on std::runtime_error will be reported for logging purposes.

## Journal AppendFile Structure

Each journal file will have the maximum size of 4MiB. The journal file will contain no header and any additional error
recovery information. The journal file consists of a continues sequence of complete journal entries, whose structure
will be defined below. Any file that is considered to be part of the journal storage but does not satisfy the structure
of a journal file will be considered as an unrecoverable and undetectable error due to the performance penalty required
to implement such checks, and will result in undefined behaviour in the implementation.

## Journal Record Structure

All record entry has an unsigned 32bit integer header in little endian. The higher 24 bits will denote the size of the
body, while the lower 8 bits will denote the message type. This allows a fast examination of the control message types
as they will have a size set to 0 as they have a fixed body size while maintaining minimal storage overhead.

The maximum payload length is 1 MiB including the header, which allows at least 4 records to be written into a file. Any
record above the size will be immediately rejected by the implementation by a reported exception.

## The Checkpoint

The checkpoint indicates that the record of an atomic and full segment of data for state recovery is complete. This
segment should be recoverable without any data from any other segment, in other words, is independent.

The implementation will drop data, with the unit of one file, any file that does not contain any data from a checkpoint
segment that has not been marked as checked, as soon as the implementation consider it is safe to do so.

## Appending Operation

All appended client data has record type 0

Appending operation is carried out asynchronously. At most one append to disk operation is active at any given time to a
directory. When a append is ongoing, any further append operation will be stored for a future batch append operation.

The data will be copied to the internal file buffer for a more simple and more consistent append operation. After
copying, the function will return immediately with a future which will notify the actual completion of the append
operation. The calling side is suggested to free up all unnecessary operation data before awaiting the future to
minimize memory footprint.

For any new file, the first record will be an automatically inserted record with type 1 and length 16, which contains 2
unsigned 64bit integers in little endian for the currently last unconfirmed checkpoint and the current checkpoint at the
time of this record is inserted. This serves as a fast indicator of which file might be valid during a file rediscovery
stage after an abnormal shutdown.

The buffer for a particular file will be a full 4MiB region borrowed directly from kls::essential's block memory pool.
The maximum amount of files that are allowed to be queued up to be committed is not limited, so please be sure that the
storage backing the system has high enough contiguous writing throughput to handle the load, and to have sufficient
system memory to act as the append buffer, or otherwise the queue could grow unbounded.

## Checkpoint Register Operation

This operation will update the internal structure of the implementation to record the file id of the current next
record, while inserting a type 1 record into the location with is the same mentioned in the appending operation, under
the condition that the current segment is not empty. The top checkpoint-id will be bumped by one.

As this is an update operation, this record will be inserted even if a previous record of type one has been queued to be
committed at the beginning of the file.

## Checkpoint Check Operation

This operation will update the internal structure of the implementation to drop the record of the first checkpoint,
while inserting a type 1 record into the location with is the same mentioned in the appending operation. The checked
checkpoint-id will be bumped by one.

As this is an update operation, this record will be inserted even if a previous record of type one has been queued to be
committed at the beginning of the file.

## Recovery walk on initialization

At the end of the initialization function, an async generator coroutine is returned so that the application can recover
any checkpoint group that has not been checked, so that the state could be recovered by replaying checkpoint sessions.
Note that this returned generator must be fully consumed to properly complete the initialization routine, though as soon
as the initialization function returns, the implementation is at a consistent state and further calls to other functions
in the implementations could be made, including the actions of replaying the old items directly onto the new instance,
as the old instance should be properly moved to a temporary directory under the storage location and opened for read
access, and all the data will be removed when after it is fully iterated.
