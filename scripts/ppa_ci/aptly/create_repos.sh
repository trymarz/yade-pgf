#!/bin/bash

set -e
for i in jammy trixie noble forky resolute
do
    aptly repo create -distribution=$i -component=main yadedaily-$i
done
