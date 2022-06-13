package com.example.client_4;

import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.multipart.MultipartFile;

import java.io.File;

@Controller
public class UploadController {
    @GetMapping("/index")
    public  String hello(){
        return "Upload";
    }

    @PostMapping("/upload")
    public ResponseEntity<?> handleFileUpload (@RequestParam("file") MultipartFile file){
        String filename = file.getOriginalFilename();

        try {
            String folderPath = System.getProperty("user.home") + "/PHOTOS/";

            file.transferTo(new File(folderPath + filename));
        }catch (Exception e){
            //return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).build();
        }
        return ResponseEntity.ok("File was uploaded successfully.");
    }
}
