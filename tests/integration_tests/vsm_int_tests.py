#!/usr/bin/env python
'''@package: vsm_integration_tests
@author: Lukasz Kostyra (l.kostyra@samsung.com)

Vasum integration tests launcher. Launches all integration tests.
'''
import unittest
from vsm_integration_tests.network_tests import *

from vsm_integration_tests.image_tests import *

# add tests here...
test_groups = [
               image_tests,
               network_tests
              ]


def main():
    for test_group in test_groups:
        print "Starting", test_group.__name__, " ..."
        suite = unittest.TestLoader().loadTestsFromModule(test_group)
        unittest.TextTestRunner(verbosity=2).run(suite)
        print "\n"

if __name__ == "__main__":
    main()
