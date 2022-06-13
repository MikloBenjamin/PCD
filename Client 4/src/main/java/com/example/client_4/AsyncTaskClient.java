package com.example.client_4;

import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Component;
import java.io.*;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.util.Scanner;
import java.util.Arrays;


@Component
public class AsyncTaskClient {

    @Async
    public void clientRun() throws IOException, InterruptedException {
        //AICI SE OBTIN PATH-URILE FIECAREI IMAGINI ALESE
        Socket s = new Socket("localhost", 7366);
        System.out.println("The client is connected to the server");
        DataOutputStream dataOutputStream = new DataOutputStream(s.getOutputStream());
        DataInputStream dataInputStream = new DataInputStream(s.getInputStream());

        String folderPath = System.getProperty("user.home") + "/PHOTOS/";
        String folderPathOutput = System.getProperty("user.home") + "/PHOTOS_OUTPUT/";

        File folder = new File(folderPath);

        File[] photos = folder.listFiles();


        Scanner input = new Scanner(System.in);
        System.out.println();
        System.out.println();
        System.out.println("Scrie cifra corespunzatoare filtrului ales");
        System.out.println("NEGATIV - 0");
        System.out.println("SEPIA - 1");
        System.out.println("BLUR -2");
        System.out.println("BLACK AND WHITE - 3");
        System.out.println("DEFAULT - 4");
        System.out.println("Alege cifra : ");
        String temp = input.nextLine();

        byte buffer[] = new byte[5];
        int x = 0;
        int y = (int) photos.length;
        int z = Integer.parseInt(temp);

        //main message
        byte messageMode[];

        //trf nr_of_imgs
        byte arr1[] = intToByteTransformation(y);
        byte arr2[] = intToByteTransformation(z);

        messageMode = ByteBuffer.allocate(5).put((byte) x).put(arr1).put(arr2).array();

        dataOutputStream.write(messageMode);

        System.out.println("NUMAR IMAGINI : " + photos.length);

        for (File file: photos) {
            //AICI INCEPE TRANSMITEREA SI PRIMIREA FIECAREI IMAGINE ALESE
            FileInputStream fileInputStream = new FileInputStream(folderPath + "/"+ file.getName());
            FileOutputStream fos = new FileOutputStream(folderPathOutput+"/"+"OUTPUT_"+file.getName());

            byte[] fileContentBytesBUFF = new byte[1021];
            byte[] packetMessage = new byte[1024];

            x = 2;
            y = numberOfPackets((int) file.length());


            byte numberOfPacketsMessage[] = new byte[3];
            byte[] serverResponse = new byte[1024];

            numberOfPacketsMessage[0] = (byte) x;
            numberOfPacketsMessage[1] = (byte) (y & 0xff);
            numberOfPacketsMessage[2] = (byte) ((y >> 8) & 0xff);


            System.out.println("Sending number of packets for " + file.getName() + " " + y);

            dataOutputStream.write(numberOfPacketsMessage);
            // Thread.sleep(1000);
            dataInputStream.read(serverResponse);

            if ((int) serverResponse[0] != 1) {
                System.out.println("Nu am primit confirmare de la server");
                dataInputStream.close();
                dataOutputStream.close();
                return;
            }
            System.out.println("Read confirmation");

            int read = 0;
            x = 3;
            while (y > 0)
            {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                read = fileInputStream.read(fileContentBytesBUFF);

                baos.write((byte) x);
                byte byte1 = (byte) (read & 0xff);
                byte byte2 = (byte) ((read >> 8) & 0xff);

                baos.write(byte1);
                baos.write(byte2);
                baos.write(fileContentBytesBUFF);

                packetMessage = baos.toByteArray();
                dataOutputStream.write(packetMessage);

                if (y <= 1)
                {
                    y--;
                    continue;
                }

                dataInputStream.read(serverResponse);

                if (serverResponse.length <= 0)
                {
                    System.out.println("0 bytes were red !");
                    dataInputStream.close();
                    dataOutputStream.close();
                    s.close();
                    return;
                }

                int responseType = (int) serverResponse[0];
                if (responseType != 1)
                {
                    System.out.println("Confirmation NOT received, received TYPE: " + responseType);
                    dataInputStream.close();
                    dataOutputStream.close();
                    s.close();
                    return;
                }
                y--;
            }



            serverResponse = new byte[1024];
            dataInputStream.read(serverResponse);
            if (serverResponse.length <= 0)
            {
                System.out.println("0 bytes were red !");
                dataInputStream.close();
                dataOutputStream.close();
                s.close();
                return;
            }

            int responseType = (int) serverResponse[0];
            if (responseType != 2)
            {
                System.out.println("NumberOfPackets NOT received, received TYPE: " + responseType);
                dataInputStream.close();
                dataOutputStream.close();
                s.close();
                return;
            }

            dataOutputStream.write((byte) 1);

            int numberOfPacketsReceived = 0;
            numberOfPacketsReceived = 0xff & (int) serverResponse[1];
            numberOfPacketsReceived |= (0xff & (int) serverResponse[2]) << 8;

            byte[] imageContentServer = new byte[1024];
            while (numberOfPacketsReceived > 0)
            {
                read = dataInputStream.read(imageContentServer);
                if (read <= 0)
                {
                    System.out.println("0 bytes were red !");
                    dataInputStream.close();
                    dataOutputStream.close();
                    s.close();
                    return;
                }
                int packetType = (int) imageContentServer[0];

                if (packetType != 3) {
                    System.out.println("SERVER X = 3 ERROR MESSAGE !");
                    dataInputStream.close();
                    dataOutputStream.close();
                    return;
                }

                dataOutputStream.write((byte) 1);
                int bytes_read = 0xff & (int)imageContentServer[1];
                bytes_read |= (0xff & (int)imageContentServer[2]) << 8;

                fos.write(imageContentServer, 3, bytes_read);

                numberOfPacketsReceived--;
            }

            System.out.println("The new image was sent");
        }
        dataInputStream.close();
        dataOutputStream.close();
        s.close();
    }

    public int numberOfPackets(int sizeOfFile){

        double numberPackets = (double) sizeOfFile / 1021;

        int integerPart = (int) numberPackets;

        double decimalPart = numberPackets - (double)integerPart;

        if (decimalPart > 0){
            return integerPart +1 ;
        } else {
            return integerPart;
        }
    }

    public byte[] intToByteTransformation(int number){
        byte byte1 = (byte) ((number) & 0xff);
        byte byte2 = (byte) ((number >> 8) & 0xff);

        byte byteArray [] = new byte[2];
        byteArray[0]= byte1;
        byteArray[1] = byte2;

        return byteArray ;
    }

}
