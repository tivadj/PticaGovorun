function y = PreEmphasis(x, preEmph)
% First order derivative of the signal. Used as a high-pass filter.
    y = zeros(size(x), 'like', x);
    y(2:end) = x(2:end) - preEmph * x(1:end-1);
    y(1) = x(1) * (1 - preEmph);
end