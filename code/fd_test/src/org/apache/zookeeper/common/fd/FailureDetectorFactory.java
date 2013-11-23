/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.zookeeper.common.fd;

import java.util.Map;

/**
 * This class represents a factory for FailureDetector instances.
 */
public class FailureDetectorFactory {

    /**
     * Creates a FailureDetector instance based on its name and its properties.
     * @param fdName
     *            the name of the failure detector
     * @param className
     *            the fully classified failure detector class name,
     *            null if fdName corresponds to a failure detector
     *            implemented by default.
     * @param properties
     *            the options of the failure detector to be created.
     * @return the created FailureDetector instance
     * @throws Exception if a failure detector instance could not be created. 
     */
    public FailureDetector createFd(String fdName, String className,
            Map<String, String> properties) throws Exception {
        
        if (fdName == null) {
            throw new IllegalArgumentException(
                    "No failure detector name was defined.");
        }
        
        Class<?> fdClass = null;
        
        if (className != null) {
            fdClass = Class.forName(className);
        } else {
            if (fdName.equals("fixed")) {
                fdClass = FixedPingFailureDetector.class;
            } else if (fdName.equals("chen")) {
                fdClass = ChenFailureDetector.class;
            } else if (fdName.equals("bertier")) {
                fdClass = BertierFailureDetector.class;
            } else if (fdName.equals("phiaccrual")) {
                fdClass = PhiAccrualFailureDetector.class;
            } else if (fdName.equals("sliced")) {
                fdClass = SlicedPingFailureDetector.class;
            } else if(fdName.equals("falcon")) {
                fdClass = FalconFailureDetector.class;
            }
        }

        if (fdClass == null) {
            throw new IllegalArgumentException("There is no corresponding " +
            		"failure detector class for name " + fdName);
        }
        
        return (FailureDetector) fdClass.getConstructor(Map.class).newInstance(properties);
    }
}
