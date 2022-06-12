package com.example.client_4;

import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Component;
import java.io.*;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.util.Scanner;


@Component
public class AsyncTaskClient {

    @Async
    public void clientRun() throws IOException, InterruptedException {
        //AICI SE OBTIN PATH-URILE FIECAREI IMAGINI ALESE
        Socket s = new Socket("localhost", 7366);
        System.out.println("The client is connected to the server");
        DataOutputStream dataOutputStream = new DataOutputStream(s.getOutputStream());
        DataInputStream dataInputStream = new DataInputStream(s.getInputStream());

        //String folderPath = "/home/antonio/PHOTOS";
        //String folderPathOutput = "/home/antonio/PHOTOS_OUTPUT";

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
        Thread.sleep(1000);



        System.out.println("NUMAR IMAGINI : " + photos.length);

        int photoCount = 0;
        for(File file : photos){
            photoCount ++;
        }

        for (File file: photos) {
            //AICI INCEPE TRANSMITEREA SI PRIMIREA FIECAREI IMAGINE ALESE
            FileInputStream fileInputStream = new FileInputStream(folderPath + "/"+ file.getName());

            // MODIFICARE



            File fileF = new File(folderPath+"/"+file.getName());




            byte[] fileContentBytesBUFF = new byte[1021];

            //AICI INCEPE TRANZACTIA CU SERVER-UL





            //AICI TRANSMITEM PRIMUL MESAJ CATRE SERVER ; SPECIFICAM MODUL


            //AICI CONSTRUIM MESAJUL IN CARE SPECIFICAM NUMARUL DE PACHETE
            x = 2;
            y = numberOfPackets((int) file.length());


            byte firstMessageSend[] = new byte[2];
            byte firstMessageReceived[] = new byte[1];
            //ByteArrayOutputStream byteConcat = new ByteArrayOutputStream();
            firstMessageSend[0] = (byte) x;
            firstMessageSend[1] = (byte) y;

            dataOutputStream.write(firstMessageSend);
            Thread.sleep(1000);
            dataInputStream.readFully(firstMessageReceived);
            if ((int) firstMessageReceived[0] != 1) {
                System.out.println("Nu am primit confirmare de la server");
                dataInputStream.close();
                dataOutputStream.close();
            } else {


                int read = 0;
                x = 3;

                byte fullImageContent1[] = new byte[1021 * numberOfPackets((int) file.length())];
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                //=====================
                int serverResponseNrPackages = 0;
                while ((read = fileInputStream.read(fileContentBytesBUFF)) > 0) {

                    // Convertes in bytes read-ul

                    byte byte1 = (byte) (read & 0xff);
                    byte byte2 = (byte) ((read >> 8) & 0xff);


                    int value1;
                    value1 = 0xff & byte1;
                    value1 |= (0xff & byte2) << 8;



                    baos.write((byte) x);
                    baos.write(byte1);
                    baos.write(byte2);
                    baos.write(fileContentBytesBUFF);


                    //byte []protocolMessage = ByteBuffer.allocate(1024).put((byte)x).put(byteRead).put(fileContentBytesBUFF).array();
                    byte[] protocolMessage = baos.toByteArray();
                    byte[] serverResponse = new byte[1024];



                    dataOutputStream.write(protocolMessage);
                    baos = new ByteArrayOutputStream();

                    dataInputStream.read(serverResponse);
                    //Thread.sleep(1000);

                    if ((int) serverResponse[0] == 1) {
                        ;

                    } else if ((int) serverResponse[0] == 2) {

                        serverResponseNrPackages = serverResponse[1];
                        //dataOutputStream.write((byte) 1);
                        Thread.sleep(1000);
                        break;
                    } else {
                        System.out.println(" Nu am primit confirmare de la server pe partea CLIENT ---- SERVER");
                        dataInputStream.close();
                        dataOutputStream.close();
                        s.close();
                    }

                }


                //CONFIRMARE CATRE SERVER CAND AM PRIMIT X = 2
                byte conf[] = new byte[1];
                conf[0] = (byte) 1;
                dataOutputStream.write(conf[0]);


                int numberOfPacketsReceived = 0;
                int numberOfPacketsToReceive = serverResponseNrPackages;

                byte[] fullImageContent;
                byte[] imageContentServer = new byte[1024];
                ByteArrayOutputStream byteArrayOutputStreamServerMessage = new ByteArrayOutputStream();


                Thread.sleep(1000);

                int count = 1;
                int packagesOf1021 = 0;

                Thread.sleep(1000);

                while ((read = dataInputStream.read(imageContentServer)) != 0) {

                    packagesOf1021++;


                    if (imageContentServer[0] != 3) {
                        System.out.println("SERVER X = 3 ERROR MESSAGE !");
                        dataInputStream.close();
                        dataOutputStream.close();
                    } else {


                        // DE CONTINUAT TRF DIN BYTE IN INT PT A AFLA Y PRIMIT DE LA SERVER

                        int number_of_packets;
                        number_of_packets = 0xff & imageContentServer[1];
                        number_of_packets |= (0xff & imageContentServer[2]) << 8;
                        //byte serverY = imageContentServer[1];

                        byte[] cont = returnProtocolZServer(imageContentServer, imageContentServer.length);
                        byteArrayOutputStreamServerMessage.write(cont);
                        if(number_of_packets < 1021){
                            numberOfPacketsReceived++;
                            dataOutputStream.write((byte) 1);
                            break;
                        }
                    }

                    numberOfPacketsReceived++;
                    dataOutputStream.write((byte) 1);

                }

                fullImageContent = byteArrayOutputStreamServerMessage.toByteArray();
                //AICI SE CREAZA IMAGINEA SI SE SALVEAZA CONFORM LOCATIEI DATE DE PATH
                System.out.println("The new image was sent");
                try (FileOutputStream fos = new FileOutputStream(folderPathOutput+"/"+"OUTPUT"+file.getName())) {
                    fos.write(fullImageContent);
                }


            }
        }
        dataInputStream.close();
        dataOutputStream.close();
        s.close();
    }





    public byte[] returnProtocolZServer(byte [] x, int xLength){
        byte y [];
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        int j = 0;
        for (int i=3; i<xLength; i++){
            byteArrayOutputStream.write(x[i]);
        }
        y=byteArrayOutputStream.toByteArray();
        return y;
    }


    public int numberOfPackets(int sizeOfFile){

        double numberPackets = (double) sizeOfFile / 1021;

        int integerPart = (int) numberPackets;

        double decimalPart = numberPackets - (double)integerPart;

        if (decimalPart > 0){
            return integerPart +1 ;
        }else{
            return integerPart;
        }

    }

    public int byteArrayLength(byte [] x){
        int countLength = 0;
        ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream(x);
        int a=0;
        while((a=byteArrayInputStream.read()) != -1){
            countLength ++;
        }
        return  countLength;
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
