#!/bin/bash

if [[ $TRAVIS_EVENT_TYPE == 'cron' || -n $TRAVIS_TAG ]]; then
	echo "Skipping testrunner.sh, cron or tag build detected"
else

	STATS_FAILED=0
	STATS_SUCCESS=0
	STATS_TOTAL=0


	# exec_test "test name" test_exec --with --args
	function exec_test()
	{
		local test_name="$1"
		shift
		(( STATS_TOTAL++ ))
		echo "execute test: '$test_name'"
		if $@
		then
			echo -e "   ... success"
			(( STATS_SUCCESS++ ))
			return 0
		else
			echo -e "   ... failed"
			(( STATS_FAILED++ ))
			return 1
		fi
		echo
	}

	######################################
	## EXEC TESTS
	cd build || exit 1

	echo
	echo "Hyperion test execution"
	echo
	exec_test "hyperiond is executable and show version" bin/hyperiond --version

	for cfg in ../config/*json*
	do
		exec_test "test $(basename $cfg)" bin/test_configfile $cfg
	done

	echo
	echo
	echo "TEST SUMMARY"
	echo "============"
	echo "    total: $STATS_TOTAL"
	echo "  success: $STATS_SUCCESS"
	echo "   failed: $STATS_FAILED"

	sleep 2

	[ $STATS_FAILED -gt 0 ] && exit 200
	exit 0
fi

