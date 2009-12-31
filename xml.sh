#!/bin/bash

xml_load()
{
        local url=$1
        [[ -z $url ]] && echo "url cannot be null" 1>&2 && exit 1

        xml=`curl -s "$1"`
}

xml_attr_index()
{
        local spec=$1
        local match=$2
        [[ -z $spec  ]] && echo "spec cannot be null"  1>&2 && exit 1
        [[ -z $match ]] && echo "match cannot be null" 1>&2 && exit 1

        echo $xml | sxpath $spec | awk -F': ' -vval="$match" '
                {
                        gsub(/^[^ ]+ /, "")
                        if ($0 == val)
                                print NR
                }
        '
}

xml_attr_select()
{
        local spec=$1
        local indices=$2
        [[ -z $spec    ]] && echo "spec cannot be null" 1>&2 && exit 1
        [[ -z $indices ]] && return 0

        echo $xml | sxpath $spec | awk -F': ' -vindices="$indices" '
                BEGIN {
                        split(indices, arr, " ")
                }
                
                {
                        for (i in arr) {
                                if (arr[i] == NR) {
                                        gsub(/^[^ ]+ /, "")
                                        gsub(/^ +/, "")
                                        gsub(/ +$/, "")
                                        print
                                }
                        }
                }
        '
}

xml_get_values()
{
        local root=$1
        local result_attr=$2
        [[ -z $root ]] && echo "root cannot be null" 1>&2 && exit 1

        shift
        shift

        args=()
        while [[ ! -z $1 ]]; do
                attr=${1%% = *}
                val=${1#* = }

                args=("${args[@]}" "$(xml_attr_index "$root@$attr" "$val")")

                shift
        done

        if [[ ${#args[@]} = 0 ]]; then
                if [[ $result_attr = "" ]]; then
                        node="$root"
                else
                        node="$root@$result_attr"
                fi
                echo $xml | sxpath "$node" | sed -r 's/^[^ ]+ //;s/^ +//;s/ +$//g'
        else
                xml_attr_select "$root@$result_attr" "$(sets_common "${args[@]}")"
        fi
}

xml_get_first_value()
{
        xml_get_values "$@" | head -n1
}
