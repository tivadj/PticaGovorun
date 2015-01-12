function y = RateOfChangeWindowed(x, windowHalf)
% Calculates Delta coefficients - how much next element
% in the sequence changes compared to the previous element.
% windowHalf: int (>=1); For windowHalf=1 this function is just 
% difference of two neighbour elements divided by 2.
assert(windowHalf >= 1)

N = length(x);
L = windowHalf;

%
denom = single(0);
for i=1:L
    denom = denom + i^2;
end
denom = 2*denom;

%
y = zeros(size(x), 'like', x);
for time=1:length(x)
    change = 0;
    for i=1:L
        if time-i >= 1
            left = x(time-i);
        else
            left = x(1);
        end
        if time+i <= N
            right = x(time+i);
        else
            right = x(end);
        end
        change = change + i * (right - left);
    end
    y(time) = change;
end

y = y ./ denom;

end
