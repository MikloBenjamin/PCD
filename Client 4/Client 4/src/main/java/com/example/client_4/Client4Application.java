package com.example.client_4;

import org.springframework.boot.CommandLineRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;
import org.springframework.core.task.SimpleAsyncTaskExecutor;
import org.springframework.core.task.TaskExecutor;
import org.springframework.scheduling.annotation.EnableAsync;

import java.io.IOException;


@SpringBootApplication
@EnableAsync
public class Client4Application{

    public static void main(String[] args) throws IOException {
        SpringApplication.run(Client4Application.class, args);


    }

}
