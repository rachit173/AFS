make write_test

# 4 KB
sudo ./write_test /users/rrt/afs/mountdir/ write_test_file.txt 4096 10 > results/write_test_4096.txt

# 64 KB
sudo ./write_test /users/rrt/afs/mountdir/ write_test_file.txt 65536 10 > results/write_test_65536.txt

# 1 MB
sudo ./write_test /users/rrt/afs/mountdir/ write_test_file.txt 1048576 10 > results/write_test_1048576.txt

# 64 MB
sudo ./write_test /users/rrt/afs/mountdir/ write_test_file.txt 67108864 10 > results/write_test_67108864.txt

