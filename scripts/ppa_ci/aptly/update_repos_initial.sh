#!/bin/bash

# First upload of the packages

PATHDEB="/home/anton/deb/"

set -e

NEW_ID=$(curl -L "https://gitlab.com/api/v4/projects/10133144/repository/commits/master" | jq --raw-output '.short_id')
echo $NEW_ID > ${PATHDEB}/OLD_ID

# Maps the apt distribution codename (used for aptly repo names below, must stay the real
# codename) to the CI job name that builds it (see .gitlab-ci.yml deb_* jobs).
declare -A CI_DEB_JOB=(
    [jammy]="deb_ubuntu_2204_jammy"
    [trixie]="deb_debian_13_trixie"
    [noble]="deb_ubuntu_2404_noble"
    [forky]="deb_debian_14_forky"
    [resolute]="deb_ubuntu_2604_resolute"
)

for i in jammy trixie noble forky resolute
do
    cd ${PATHDEB}
    wget https://gitlab.com/api/v4/projects/10133144/jobs/artifacts/master/download?job=${CI_DEB_JOB[$i]} -O yade.zip
    unzip yade.zip
    aptly repo remove yadedaily-$i 'yadedaily'
    aptly repo remove yadedaily-$i 'libyadedaily'
    aptly repo remove yadedaily-$i 'yadedaily-doc'
    aptly repo remove yadedaily-$i 'python3-yadedaily'
    aptly repo add yadedaily-$i deb/*.deb
    aptly repo add yadedaily-$i deb/*.dsc
    rm -rf ${PATHDEB}/*
    aptly publish repo yadedaily-$i filesystem:yadedaily:
done
