#!/usr/bin/awk
#
# Convert time interval specifications with suffixes to an integer number of
# seconds.
{
	nr = $1;
	if ($2 == "" || $2 ~ /s/)	# seconds
		;
	else if ($2 ~ /m/)		# minutes
		nr *= 60;
	else if ($2 ~ /h/)		# hours
		nr *= 3600;
	else if ($2 ~ /d/)		# days
		nr *= 86400;
	else if ($2 ~ /w/)		# weeks
		nr *= 604800;
	else {
		printf("%s: unknown suffix\n", $2);
		exit 1;
	}

	printf("%d\n", nr);
}
