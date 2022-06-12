
./client_2 -p ~/Pictures/Client2_1/ -n &
./client_2 -p ~/Pictures/Client2_2/ -b &
./client_2 -p ~/Pictures/Client2_3/ -w &
./client_2 -p ~/Pictures/Client2_4/ -s &

wait

python3 client_3.py -fi ~/Pictures/Client3_images/ -fpi ~/Pictures/Client3_processed/ -n -outlog client1_log.txt &
python3 client_3.py -fi ~/Pictures/Client3_images/ -fpi ~/Pictures/Client3_processed/ -b -outlog client2_log.txt &
python3 client_3.py -fi ~/Pictures/Client3_images/ -fpi ~/Pictures/Client3_processed/ -w -outlog client3_log.txt &
python3 client_3.py -fi ~/Pictures/Client3_images/ -fpi ~/Pictures/Client3_processed/ -s -outlog client4_log.txt &

wait
printf "end of script\n"
