#!/usr/bin/python
# -*- coding: utf-8 -*-

# Python script to find the largest files in a git repository.
# The general method is based on the script in this blog post:
# http://stubbisms.wordpress.com/2009/07/10/git-script-to-show-largest-pack-objects-and-trim-your-waist-line/
#
# The above script worked for me, but was very slow on my 11GB repository. This version has a bunch
# of changes to speed things up to a more reasonable time. It takes less than a minute on repos with 250K objects.
#
# The MIT License (MIT)
# Copyright (c) 2015 Nick Kocharhook
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and 
# associated documentation files (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge, publish, distribute,
# sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or
# substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT 
# NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT 
# OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


from subprocess import check_output, CalledProcessError, Popen, PIPE
import argparse
import signal
import sys

sortByOnDiskSize = False

def main():
	global sortByOnDiskSize

	signal.signal(signal.SIGINT, signal_handler)
	
	args = parseArguments()
	sortByOnDiskSize = args.sortByOnDiskSize
	sizeLimit = 1024*args.filesExceeding

	if args.filesExceeding > 0:
		print ("Finding objects larger than {}k...", args.filesExceeding)
	else:
		print ("Finding the {} largest objects...", args.matchCount)

	blobs = getTopBlobs(args.matchCount, sizeLimit)

	populateBlobPaths(blobs)
	printOutBlobs(blobs)

def getTopBlobs(count, sizeLimit):
	sortColumn = 4
	
	if sortByOnDiskSize:
		sortColumn = 3

	verifyPack = "git verify-pack -v `git rev-parse --git-dir`/objects/pack/pack-*.idx | grep blob | sort -k{}nr".format(sortColumn)
	output = check_output(verifyPack, shell=True).split("\n")[:-1]

	blobs = dict()
	compareBlob = Blob("a b {} {} c".format(sizeLimit, sizeLimit)) # use __lt__ to do the appropriate comparison

	for objLine in output:
		blob = Blob(objLine)

		if sizeLimit > 0:
			if compareBlob < blob:
				blobs[blob.sha1] = blob
			else:
				break
		else:
			blobs[blob.sha1] = blob

			if len(blobs) == count:
				break

	return blobs


def populateBlobPaths(blobs):
	if len(blobs):
		print ("Finding object paths")

		# Only include revs which have a path. Other revs aren't blobs.
		revList = "git rev-list --all --objects | awk '$2 {print}'"
		allObjectLines = check_output(revList, shell=True).split("\n")[:-1]

		outstandingKeys = blobs.keys()

		for line in allObjectLines:
			cols = line.split()
			sha1, path = cols[0], " ".join(cols[1:])

			if (sha1 in outstandingKeys):
				outstandingKeys.remove(sha1)
				blobs[sha1].path = path

				# short-circuit the search if we're done
				if not len(outstandingKeys):
					break


def printOutBlobs(blobs):
	if len(blobs):
		csvLines = ["size,pack,hash,path"]

		for blob in sorted(blobs.values(), reverse=True):
			csvLines.append(blob.csvLine())

		p = Popen(["column", "-t", "-s", "','"], stdin=PIPE, stdout=PIPE, stderr=PIPE)
		stdout, stderr = p.communicate("\n".join(csvLines))

		print ("\nAll sizes in kB. The pack column is the compressed size of the object inside the pack file.\n")
		print (stdout.rstrip('\n'))
	else:
		print ("No files found which match those criteria.")


def parseArguments():
	parser = argparse.ArgumentParser(description='List the largest files in a git repository')
	parser.add_argument('-c', '--match-count', dest='matchCount', type=int, default=10,
						help='The number of files to return. Default is 10. Ignored if --files-exceeding is used.')
	parser.add_argument('--files-exceeding', dest='filesExceeding', type=int, default=0,
						help='The cutoff amount, in KB. Files with a pack size (or pyhsical size, with -p) larger than this will be printed.')
	parser.add_argument('-p', '--physical-sort', dest='sortByOnDiskSize', action='store_true', default=False,
						help='Sort by the on-disk size of the files. Default is to sort by the pack size.')

	return parser.parse_args()


def signal_handler(signal, frame):
    print('Caught Ctrl-C. Exiting.')
    sys.exit(0)


class Blob(object):
	sha1 = ''
	size = 0
	packedSize = 0
	path = ''

	def __init__(self, line):
		cols = line.split()
		self.sha1, self.size, self.packedSize = cols[0], int(cols[2]), int(cols[3])

	def __repr__(self):
		return '{} - {} - {} - {}'.format(self.sha1, self.size, self.packedSize, self.path)

	def __lt__(self, other):
		if (sortByOnDiskSize):
			return self.size < other.size
		else:
			return self.packedSize < other.packedSize

	def csvLine(self):
		return "{},{},{},{}".format(self.size/1024, self.packedSize/1024, self.sha1, self.path)


# Default function is main()
if __name__ == '__main__':
	main()
