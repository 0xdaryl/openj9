package org.openj9.test.attachAPI;

import static org.openj9.test.attachAPI.TestConstants.TARGET_VM_CLASS;
import static org.openj9.test.util.StringUtilities.searchSubstring;
import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.fail;
import static org.testng.AssertJUnit.assertTrue;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.Properties;
import java.util.stream.Collectors;

import org.openj9.test.util.HelloWorld;
import org.testng.annotations.AfterTest;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;



import org.testng.annotations.Test;
@Test(groups = { "level.extended" })
public class TestJmap extends AttachApiTest {
    @Test
    public void testDummy() {
        log("Dummy test for IBM"); //$NON-NLS-1$
    }
}

