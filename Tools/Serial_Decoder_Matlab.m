% Raphael BOICHOT 30/01/2023
% for any question : raphael.boichot@gmail.com

clear
clc
disp('-----------------------------------------------------')
disp('|Beware, this code is for Matlab ONLY !!!           |')
disp('|You can break the code with Ctrl+C on Editor Window|')
disp('-----------------------------------------------------')
% pkg load image
% pkg load instrument-control
arduinoObj = serialport("COM4",2000000); %set the Arduino com port here
%set(arduinoObj, 'timeout',-1);
flag=0;
image_counter=0;
mkdir ./image
while flag==0 %infinite loop
    data = readline(arduinoObj);
    if not(strlength(data)>100)
        disp(data);
    end
    
    if strlength(data)>1000 %This is an image coming
        data=(char(data));
        offset=1; %First byte is always junk (do not know why, probably a Println LF)
        im=[];
        if length(data)>=16385
            for i=1:1:120 %We get the full image, 5 lines are junk at bottom, top is glitchy due to amplifier artifacts
                for j=1:1:128
                    im(i,j)=hex2dec(data(offset:offset+1));
                    offset=offset+3;
                end
            end
            raw=im; %We keep the raw data just in case
            maximum=max(max(raw(1:120,:)));
            minimum=min(min(raw(1:120,:)));
            moyenne=mean(mean(raw(1:120,:)));
            subplot(1,2,1);
            histogram(raw(1:120,:),100)
            title(["min ",num2str(minimum),"max ",num2str(maximum)])
        end
        image_display=uint8((raw-128)*2);
        image_counter=image_counter+1;
        image_display=imresize(image_display,4,"nearest");
        imwrite(image_display,['./image/output_',num2str(image_counter),'.gif'],'gif');
        subplot(1,2,2);
        imshow(image_display)
        drawnow
    end
end
