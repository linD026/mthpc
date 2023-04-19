#FILE=log-test-urcu-$(shell date +"%Y-%m-%d")
FILE=log-test-urcu-timing-$(shell date +"%Y-%m-%d")
LOG_FILE=logs/$(FILE)

#TEST := test_urcu.c
TEST := test_urcu_timing.c

urcu:
	gcc -o test $(TEST) -DDYNAMIC_LINK_TEST -lurcu

mthpc:
	gcc -o test $(TEST) ../../libmthpc.so -lurcu -I../../include -DUSE_MTHPC_LIB 

run:
	./test 100 1 10

clean:
	rm -f test

gen-log:
	touch $(LOG_FILE)
