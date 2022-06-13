package com.example.client_4;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

@RestController
public class RelationController {

    @Autowired
    private AsyncTaskClient asyncTaskClient;

    @RequestMapping("/start")
    public Map<String, String> callAsyncMethod() throws IOException, InterruptedException {


        asyncTaskClient.clientRun();

        return new HashMap<String, String>();  // returns empty braces
    }
}
