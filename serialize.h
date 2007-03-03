#ifndef SERIALIZE_H
#define SERIALIZE_H

void queue_serializable_job(struct sftpjob *job);
/* Called for SSH_FXP_READ and SSH_FXP_WRITE to establish the job's place in
 * the serialization queue */

void serialize_on_handle(struct sftpjob *job, 
			 int istext);
/* Wait until there are no olderjobs address the same offset via the same
 * handle */

void serialize_remove_job(struct sftpjob *job);
/* Called when any job is completed to remove it from the serialization
 * queue */

#endif /* SERIALIZE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
