#/bin/bash
echo test starts!
./2 6666 > res &
echo server starts successfully!
sleep 0.5
nc 127.0.0.1 6666 > res0 &
echo client0 starts successfully!
sleep 0.5
for((i=1;i<=31;i++));  
do   
	nc 127.0.0.1 6666 > res$i < test$i &
	echo client$i starts successfully!  
	sleep 0.1
done  
sleep 20
echo now I will kill the threads.
for((i=2;i<=40;i++));
do
	kill %$i
done
sleep 1
kill %1
exit