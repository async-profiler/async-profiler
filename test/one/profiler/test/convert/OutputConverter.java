/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test.convert;

import one.convert.Arguments;

import java.io.IOException;

/*
Interface to be implemented by various profile output converters in tests.
 */
public interface OutputConverter<I, O> {

    O convert(I input, Arguments args) throws IOException;
}
