clc
clear all
close all

% car wheel parameters

% oda paper
% rho = 1800;
% radius = 4e-3;
% mass = rho*4/3*pi*radius^3;
% inertia = 0.5*mass*radius^2;

FontSize = 36;
LineWidth = 4;


mass = 5;
radius = 0.2;
inertia = 0.5*mass*radius^2;


gravity = 9.8;
mu_s = 0.25;
mu_k = 0.2;

scenario = 'disk_rolling';
solver = 'explicit';
%Tend = 1.5; dt = 1e-4; t = 0:dt:Tend;

Tend = 5; dt = 1e-4; t = 0:dt:Tend;


tech_report = false;
tech_report_dir = '/Users/lulu/Documents/TechReports/Friction3D/Images/';

% initialize kinematics array
pos = zeros(length(t),1); velo = zeros(length(t),1); acc = zeros(length(t),1);
theta = zeros(length(t),1); omic = zeros(length(t),1); alpha = zeros(length(t),1);

% sliding/rolling friction array
Ef_array = zeros(length(t),1); Df_array = zeros(length(t),1);
Te_array = zeros(length(t),1); Td_array = zeros(length(t),1);

% initial condition
velo(1) = 0.5; omic(1) = 0;

% slide/roling friction parameter
% rolling friction parameters
%eta = 0.3;
%Kr = eta*R^2*Ke;   % rolling friction stiffness


% sliding friction model 
Ke = 1e5; % spring stiffness
%Ke = 4e7; % from Oda paper
sliding_mode = 'k'; % sliding friction mode
Sij = 10; Sij_array = zeros(length(t),1); Sij_array(1) = 0;  % relative displacement
delta_Sij = 0; dSij_array = zeros(length(t),1); dSij_array(1) = 0; % relative displacement increment 
sliding_friction = 0; % initialzie sliding friction
sliding_slack_s = mu_s*mass*gravity/Ke; % static slack
sliding_slack_k = mu_k*mass*gravity/Ke; % kinetic slack

% rolling friction model
Kr = 1600;
%Kr = 700; %oda experiment
C_cr = 2*sqrt(inertia*Kr);
eta_r = 1;
eta_t = 1;
Cr = eta_r * C_cr;
rolling_mode = 'k'; % initialze rolling mode
rolling_slack_s = sliding_slack_s/(2*radius);  % slack for sphere on plane, static
rolling_slack_k = sliding_slack_k/(2*radius); % slack for sphere on plane, kinetic
rolling_torque = 0; rolling_torque_array = zeros(length(t),1); % initialize rolling torque
rolling_history = 10; rolling_history_array = zeros(length(t),1); % relative rolling history
excursion = 0;

check_excursion = zeros(length(t),1);
for i = 1:length(t)-1
    
    if solver == 'explicit'
        acc(i+1) = -sliding_friction/mass;
        alpha(i+1) = (sliding_friction*radius - rolling_torque)/inertia ;  % don't simplify the equation, use inertia instead of mass*radius, confusing...
    end
    
     if t(i) > 0 && t(i)<0.05
        fprintf('t=%g, Sij=%g, d_Sij=%g, Fr(%s):%g=%g+%g, Tr(%s):%g=%g+%g, theta_ij=%g, velo=%g, omic=%g\n',...
                 t(i), Sij,    delta_Sij, sliding_mode, -sliding_friction, -Ef, -Df, rolling_mode, rolling_torque, Te, torque_damping, rolling_history, velo(i), omic(i));
     end
        
    % explicit kinematics update
    velo(i+1) = velo(i) + dt*acc(i+1);
    pos(i+1) = pos(i) + dt*velo(i+1);
    omic(i+1) = omic(i) + dt*alpha(i+1);
    theta(i+1) = theta(i) + dt*omic(i+1);
    
    pj = theta(i+1)*radius - theta(i)*radius;
    pi = pos(i+1) - pos(i);
    
    % relative slide and roll
    delta_Sij = pi-pj;  % ground minus the sphere
    Sij = Sij + delta_Sij;
    excursion = (pj)/radius;
    rolling_history = rolling_history + excursion;
    
    slide_slack = abs(Sij);
    Sij_old = Sij;
    rolling_history_old = rolling_history;

    % sliding friction mode and magnitude
    if sliding_mode == 's'
        Kd = 2*sqrt(mass*Ke);
        alpha_s = slide_slack/sliding_slack_s;
        if alpha_s > 1
            Sij = Sij/alpha_s;
            sliding_mode = 'k';
            Kd = 0;
        end
    else
        alpha_k = slide_slack/sliding_slack_k;
        if alpha_k > 1
            Sij = sign(Sij)*sliding_slack_k;
            Kd = 0;
        else
            sliding_mode = 's';
            Kd = eta_t*2*sqrt(mass*Ke);
        end
    end
    
    % rolling friction mode and magnitude
    if rolling_mode == 's'
        alpha_s = abs(rolling_history)/rolling_slack_s;
        torque_damping = Cr * omic(i+1);
%             omic(i+1)
%             torque_damping

        if alpha_s > 1
            rolling_history = rolling_history/alpha_s;
            rolling_mode = 'k';
            torque_damping = 0;
        end
    else
        alpha_k = abs(rolling_history)/rolling_slack_k;
        if alpha_k > 1
            rolling_history = sign(rolling_history)*rolling_slack_k;
            torque_damping = 0;
        else
            rolling_mode = 's';
            torque_damping = Cr * omic(i+1);
%             omic(i+1)
%             torque_damping
        end
    end
    
%    fprintf('time=%.5f, rolling_history=%g, rolling_slack_k=%g, rolling_slack_s=%g, Td=%g, omic=%g\n', t(i), rolling_history, rolling_slack_k, rolling_slack_s, torque_damping, omic(i+1));
    
    Ef = Ke * Sij;
    if sliding_mode == 's'
        Df = eta_t*Kd * delta_Sij/dt;
%         delta_Sij
%         Df
    else
        Df = 0;
    end
    
    sliding_friction = Ef + Df;
    
    rolling_torque = Kr * rolling_history + torque_damping;
    Ef_array(i+1) = Ef;
    Df_array(i+1) = Df;
    Sij_array(i+1) = Sij;
    dSij_array(i+1) = delta_Sij;
    Te = Kr * rolling_history;
    Te_array(i+1) = Te;
    Td_array(i+1) = torque_damping;
    rolling_torque_array(i+1) = rolling_torque;
    rolling_history_array(i+1) = rolling_history;
    
%    fprintf('time=%.5f, slide_mode=%s, excursion=%g\n', t(i), sliding_mode, excursion )

end


if tech_report == false
FontSize = 22;
LineWidth = 2;

t_start = Tend - 0.5;
figure('units','normalized','outerposition',[0 0 1 1]);

% subplot(2,3,1)
% makePlot(t,pos,'time (ms)','position of CM (m)',sprintf('%s solver', solver), LineWidth, FontSize);
% subplot(2,3,2)
% makePlot(t,velo,'time (ms)','velocity of CM (m/s)',sprintf('\\mu_s=%.2f,\\mu_k=%.2f', mu_s, mu_k), LineWidth, FontSize);
% subplot(2,3,3)
% makePlot(t,acc,'time (ms)','acceleration of CM (m/s^2)', '', LineWidth, FontSize);
% 
% subplot(2,3,4)
% makePlot(t,theta,'time (ms)','angular position (rad)', '', LineWidth, FontSize);
% subplot(2,3,5)
% makePlot(t,omic,'time (ms)','angular velocity (rad/s)', '', LineWidth, FontSize);
% subplot(2,3,6)
% makePlot(t,alpha,'time (ms)','angular acceleration (rad/s^2)', '', LineWidth, FontSize);
subplot(2,3,1)
makePlotYY(t,pos,t,theta ,'time (sec)', 'position of CM (m)', 'angular position (rad)', '2D', LineWidth, FontSize)
xlim([t_start, Tend]);

subplot(2,3,2)
makePlotYY(t,velo,t,omic ,'time (sec)', 'velocity of CM (m/s)', 'angular velocity (rad/s)', sprintf('\\mu_s=%.2f,\\mu_k=%.2f', mu_s, mu_k), LineWidth, FontSize)
xlim([t_start, Tend]);

subplot(2,3,3)
makePlotYY(t,acc,t,alpha ,'time (sec)', 'acceleration of CM (m/s^2)', 'angular acceleration (rad/s^2)', sprintf('dt=%g', dt), LineWidth, FontSize)
xlim([t_start, Tend]);

%xlim([9.6,10.5]);
% more plots on friction
subplot(2,3,4)
makePlotYY(t, Ef_array, t, Te_array, 'time (sec)', 'F_E (N)', 'T_E (Nm)', sprintf('(Fr_s,Fr_k) = (%.0f,%.0f)N', mu_s*mass*gravity, mu_k*mass*gravity), LineWidth, FontSize)
xlim([t_start, Tend]);

subplot(2,3,5)
makePlotYY(t, Df_array, t, Td_array, 'time (sec)', 'F_D (N)', 'T_D (Nm)',  sprintf('D_t=%.0fN/(m/s), D_r=%.0fNm/(rad/s)', sqrt(mass*Ke)*eta_t, Cr*eta_r), LineWidth, FontSize);
xlim([t_start, Tend]);

subplot(2,3,6)
makePlotYY(t, Sij_array*1e3, t, rolling_history_array*1e3, 'time (sec)', 'S_{ij} (mm)','\Theta_{ij} (\times10^{-3}rad)', 'relative motion', LineWidth, FontSize);
xlim([t_start, Tend]);

end

if tech_report == true
    
    t_1 = 0.2; % time switch into rolling without slipping
    t_2 = 1.2; % time disk comes to a stop
    
    
plotHeight = 0.7;
figure('units','normalized','outerposition',[0 0 1 plotHeight]);
str_figname = strcat(scenario, '_pos.png');
%str_figname = strcat(scenario, '_pos.png');
subplot(1,2,1)
makePlotYY(t, pos, t, theta ,'time(sec)', '$${x}$$ (m)', '$${\theta}$$ (rad)', '', LineWidth, FontSize)


subplot(1,4,3)
yyaxis left
plot(t, Ef_array, 'LineWidth', LineWidth);
ylabel('$$\mathbf{E}_f$$', 'Interpreter', 'latex', 'FontSize', FontSize);
xlim([0,t_1])
ylim(1.01*[min(Ef_array), max(Ef_array)]);



yyaxis right
plot(t, Te_array, 'LineWidth', LineWidth);
box off
xlim([0,t_1])
ylim(1.01*[min(Te_array), max(Te_array)]);

set(gca, 'linewidth', LineWidth);
a = get(gca, 'XTick');
set(gca, 'FontSize', FontSize-5)
set(gca, 'linewidth', LineWidth);
set(gca, 'FontSize', FontSize-5)
set(gca, 'Ycolor', 'w')
set(gca, 'YTick', []);


hAxis(2)=subplot(1,4,4);
yyaxis left
plot(t, Ef_array, 'LineWidth', LineWidth);
box off
xlim([t_2,Tend])
ylim(1.01*[min(Ef_array), max(Ef_array)]);
set(gca, 'Ycolor', 'w')
set(gca, 'YTick', []);


yyaxis right
plot(t, Te_array, 'LineWidth', LineWidth);
box off
xlim([t_2,Tend])
ylim(1.01*[min(Te_array), max(Te_array)]);

set(gca, 'linewidth', LineWidth);
a = get(gca, 'XTick');
set(gca, 'FontSize', FontSize-5)
set(gca, 'linewidth', LineWidth);
set(gca, 'FontSize', FontSize-5)
ylabel('$$\mathcal{T}_e$$', 'Interpreter', 'latex', 'FontSize', FontSize);
h_xlabel = xlabel('time(sec)', 'FontSize', FontSize);
set(h_xlabel, 'Units', 'normalized')
set(h_xlabel, 'Position', get(h_xlabel, 'Position') +[-0.55, 0.02 0])


annotation(figure(1),'line',[0.7 0.75],...
    [0.11 0.11],'LineWidth',3,'LineStyle',':');
annotation(figure(1),'line',[0.7 0.75],...
    [0.185 0.185],'Color',[0.85 0.33 0.1],'LineWidth',3,...
    'LineStyle','--');
annotation(figure(1),'line',[0.7 0.75],...
    [0.915 0.915],'Color',[0 0.45 0.74],'LineWidth',3,...
    'LineStyle','--');%print(gcf, strcat(tech_report_dir, str_figname), '-dpng', '-r300');



figure('units','normalized','outerposition',[0 0 1 plotHeight]);
str_figname = strcat(scenario, '_velo.png');
subplot(1,2,1)
makePlotYY(t,velo,t,omic ,'time (sec)', '$$\dot{x}$$ (m/s)', '$$\dot{\theta}$$ (rad/s)', '', LineWidth, FontSize)


subplot(1,4,3)
yyaxis left
plot(t, Df_array, 'LineWidth', LineWidth);
ylabel('$$\mathbf{D}_f$$', 'Interpreter', 'latex', 'FontSize', FontSize);
xlim([0,t_1])
ylim(1.01*[min(Df_array), 1.2*max(Df_array)]);



yyaxis right
plot(t, Td_array, 'LineWidth', LineWidth);
box off
xlim([0,t_1])
ylim(1.01*[min(Td_array), 1.2*max(Df_array)]);

set(gca, 'linewidth', LineWidth);
a = get(gca, 'XTick');
set(gca, 'FontSize', FontSize-5)
set(gca, 'linewidth', LineWidth);
set(gca, 'FontSize', FontSize-5)
set(gca, 'Ycolor', 'w')
set(gca, 'YTick', []);


hAxis(2)=subplot(1,4,4);
yyaxis left
plot(t, Df_array, 'LineWidth', LineWidth);
box off
xlim([t_2,Tend])
ylim(1.01*[min(Df_array), 1.2*max(Df_array)]);
set(gca, 'Ycolor', 'w')
set(gca, 'YTick', []);


yyaxis right
plot(t, Td_array, 'LineWidth', LineWidth);
box off
xlim([t_2,Tend])
ylim(1.01*[min(Td_array), 1.2*max(Df_array)]);

set(gca, 'linewidth', LineWidth);
a = get(gca, 'XTick');
set(gca, 'FontSize', FontSize-5)
set(gca, 'linewidth', LineWidth);
set(gca, 'FontSize', FontSize-5)
ylabel('$$\mathcal{T}_D$$', 'Interpreter', 'latex', 'FontSize', FontSize);
h_xlabel = xlabel('time(sec)', 'FontSize', FontSize);
set(h_xlabel, 'Units', 'normalized')
set(h_xlabel, 'Position', get(h_xlabel, 'Position') +[-0.55, 0.02 0])


annotation(figure(2),'line',[0.7 0.75],...
    [0.11 0.11],'LineWidth',3,'LineStyle',':');
annotation(figure(2),'line',[0.7 0.75],...
    [0.185 0.185],'Color',[0.85 0.33 0.1],'LineWidth',3,...
    'LineStyle','--');
annotation(figure(2),'line',[0.7 0.75],...
    [0.915 0.915],'Color',[0 0.45 0.74],'LineWidth',3,...
    'LineStyle','--');


% figure('units','normalized','outerposition',[0 0 1 plotHeight]);
% str_figname = strcat(scenario, '_acc.png');
% subplot(1,2,1)
% makePlotYY(t,acc,t,alpha ,'time (ms)', '$$\ddot{x}$$', '$$\ddot{\theta}$$', '' , LineWidth, FontSize)
% subplot(1,2,2);
% makePlotYY(t, Sij_array*1e3, t, rolling_history_array*1e3, 'time (ms)', '$$S_{ij}(\times10^{-3})$$','$$\Theta_{ij}(\times10^{-3})$$', '', LineWidth, FontSize);
% set(gcf,'color','w');

figure('units','normalized','outerposition',[0 0 1 plotHeight]);
str_figname = strcat(scenario, '_acc.png');
subplot(1,4,1)
yyaxis left
plot(t, acc, 'LineWidth', LineWidth);
ylabel('$$\ddot{x}$$', 'Interpreter', 'latex', 'FontSize', FontSize);
xlim([0,t_1])
ylim(1.01*[min(acc), max(acc)]);

yyaxis right
plot(t, alpha, 'LineWidth', LineWidth);
box off
xlim([0,t_1])
ylim(1.01*[min(alpha), max(alpha)]);

set(gca, 'linewidth', LineWidth);
a = get(gca, 'XTick');
set(gca, 'FontSize', FontSize-5)
set(gca, 'linewidth', LineWidth);
set(gca, 'FontSize', FontSize-5)
set(gca, 'Ycolor', 'w')
set(gca, 'YTick', []);


hAxis(2)=subplot(1,4,2);
yyaxis left
plot(t, acc, 'LineWidth', LineWidth);
box off
xlim([t_2,Tend])
ylim(1.01*[min(acc), max(acc)]);
set(gca, 'Ycolor', 'w')
set(gca, 'YTick', []);

yyaxis right
plot(t, alpha, 'LineWidth', LineWidth);
box off
xlim([t_2,Tend])
ylim(1.01*[min(alpha), max(alpha)]);
set(gca, 'linewidth', LineWidth);
a = get(gca, 'XTick');
set(gca, 'FontSize', FontSize-5)
set(gca, 'linewidth', LineWidth);
set(gca, 'FontSize', FontSize-5)
ylabel('$$\ddot{\theta}$$', 'Interpreter', 'latex', 'FontSize', FontSize);
h_xlabel = xlabel('time(sec)', 'FontSize', FontSize);
set(h_xlabel, 'Units', 'normalized')
set(h_xlabel, 'Position', get(h_xlabel, 'Position') +[-0.55, 0.02 0])


annotation(figure(3),'line',[0.7 0.75],...
    [0.11 0.11],'LineWidth',3,'LineStyle',':');
annotation(figure(3),'line',[0.7 0.75],...
    [0.185 0.185],'Color',[0.85 0.33 0.1],'LineWidth',3,...
    'LineStyle','--');
annotation(figure(3),'line',[0.7 0.75],...
    [0.915 0.915],'Color',[0 0.45 0.74],'LineWidth',3,...
    'LineStyle','--');

subplot(1,2,2)
yyaxis left
plot(t, Sij_array*1e3, 'LineWidth', LineWidth);
ylabel('$$S_{ij}(\times10^{-3})$$', 'Interpreter', 'latex', 'FontSize', FontSize);
%ylim(1.01*[min(rolling_history_array*1e6), max(rolling_history_array*1e3)]);
yyaxis right
plot(t, rolling_history_array*1e3, 'LineWidth', LineWidth);
ylabel('$$\Theta_{ij}(\times10^{-3})$$', 'Interpreter', 'latex', 'FontSize', FontSize);

%xlim([0,t_1])
%ylim(1.01*[min(rolling_history_array*1e6), max(rolling_history_array*1e6)]);

set(gca, 'linewidth', LineWidth);
a = get(gca, 'XTick');
set(gca, 'FontSize', FontSize-5)
set(gca, 'linewidth', LineWidth);
set(gca, 'FontSize', FontSize-5)


%imwrite(img.cdata, strcat(tech_report_dir, str_figname)); % save figure exactly how it is

end
