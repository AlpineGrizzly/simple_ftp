#!/bin/bash
./run_client.sh & 
./do_bigfile.sh & 
./another_run_client.sh & 
