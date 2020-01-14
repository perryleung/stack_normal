IP="218.192.170.124 218.192.168.116"
NODE="192.168.2.101 192.168.2.102 192.168.2.103 192.168.2.104 192.168.2.105 "

for i in $NODE
do
    #echo "ping " $i
    ping=`ping -c 1 $i|grep loss|awk '{print $6}'|awk -F "%" '{print $1}'`
    if [ $ping=100 ];then
        echo ping $i fail
    else
        echo ping $i ok
    fi
done

